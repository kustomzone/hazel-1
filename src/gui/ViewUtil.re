module Regexp = Js_of_ocaml.Regexp;
open SemanticsCommon;
open GeneralUtil;

exception MalformedView(int);

[@deriving sexp]
type delim_path = (Path.steps, delim_index);
[@deriving sexp]
type op_path = (Path.steps, op_index);

let node_id = steps =>
  "node__" ++ Sexplib.Sexp.to_string(Path.sexp_of_steps(steps));
let text_id = steps =>
  "text__" ++ Sexplib.Sexp.to_string(Path.sexp_of_steps(steps));
let path_id = path =>
  "path__" ++ Sexplib.Sexp.to_string(Path.sexp_of_t(path));
let delim_id = (steps, delim_index) =>
  "delim__"
  ++ Sexplib.Sexp.to_string(sexp_of_delim_path((steps, delim_index)));
let op_id = (steps, op_index) =>
  "op__" ++ Sexplib.Sexp.to_string(sexp_of_op_path((steps, op_index)));

// necessary to pre-process our ids before using them to
// construct CSS selectors because they contain parens characters
// and these are special in selector syntax
let escape_parens = s =>
  range(s |> String.length)
  |> List.map(i =>
       switch (s.[i]) {
       | '(' => "\\("
       | ')' => "\\)"
       | c => c |> String.make(1)
       }
     )
  |> List.fold_left((acc, s) => acc ++ s, "");

let box_node_indicator_id = "box_node_indicator";
let child_indicator_id = i => "child_indicator__" ++ string_of_int(i);
let box_tm_indicator_id = "box_tm_indicator";
let seq_tm_indicator_id = i => "seq_tm_indicator__" ++ string_of_int(i);
let op_node_indicator_id = "op_node_indicator";

let steps_of_node_id = s =>
  switch (Regexp.string_match(Regexp.regexp("^node__(.*)$"), s, 0)) {
  | None => None
  | Some(result) =>
    switch (Regexp.matched_group(result, 1)) {
    | None => None
    | Some(ssexp) =>
      Some(Path.steps_of_sexp(Sexplib.Sexp.of_string(ssexp)))
    }
  };
let steps_of_text_id = s =>
  switch (Regexp.string_match(Regexp.regexp("^text__(.*)$"), s, 0)) {
  | None => None
  | Some(result) =>
    switch (Regexp.matched_group(result, 1)) {
    | None => None
    | Some(ssexp) =>
      Some(Path.steps_of_sexp(Sexplib.Sexp.of_string(ssexp)))
    }
  };
let path_of_path_id = s =>
  switch (Regexp.string_match(Regexp.regexp("^path__(.*)$"), s, 0)) {
  | None => None
  | Some(result) =>
    switch (Regexp.matched_group(result, 1)) {
    | None => None
    | Some(ssexp) => Some(Path.t_of_sexp(Sexplib.Sexp.of_string(ssexp)))
    }
  };
let delim_path_of_delim_id = s =>
  switch (Regexp.string_match(Regexp.regexp("^delim__(.*)$"), s, 0)) {
  | None => None
  | Some(result) =>
    switch (Regexp.matched_group(result, 1)) {
    | None => None
    | Some(ssexp) =>
      Some(delim_path_of_sexp(Sexplib.Sexp.of_string(ssexp)))
    }
  };