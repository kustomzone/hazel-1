open SemanticsCommon;
open GeneralUtil;

[@deriving sexp]
type opseq_surround = OperatorSeq.opseq_surround(UHPat.t, UHPat.op);
type opseq_prefix = OperatorSeq.opseq_prefix(UHPat.t, UHPat.op);
type opseq_suffix = OperatorSeq.opseq_suffix(UHPat.t, UHPat.op);

[@deriving sexp]
type t =
  | CursorP(cursor_position, UHPat.t)
  /* zipper cases */
  | ParenthesizedZ(t)
  | OpSeqZ(UHPat.skel_t, t, opseq_surround)
  | InjZ(err_status, inj_side, t);

exception SkelInconsistentWithOpSeq;

let valid_cursors = (p: UHPat.t): list(cursor_position) =>
  switch (p) {
  | EmptyHole(_) => delim_cursors(1)
  | Wild(_) => delim_cursors(1)
  | Var(_, _, x) => text_cursors(Var.length(x))
  | NumLit(_, n) => text_cursors(num_digits(n))
  | BoolLit(_, b) => text_cursors(b ? 4 : 5)
  | ListNil(_) => delim_cursors(1)
  | Inj(_, _, _) => delim_cursors(2)
  | Parenthesized(_) => delim_cursors(2)
  | OpSeq(_, seq) =>
    range(OperatorSeq.seq_length(seq))
    |> List.map(k => k + 1)
    |> List.map(k => delim_cursors_k(k))
    |> List.flatten
  };

let is_valid_cursor = (cursor: cursor_position, p: UHPat.t): bool =>
  contains(valid_cursors(p), cursor);

let bidelimit = (zp: t): t =>
  switch (zp) {
  | CursorP(_, p) =>
    if (UHPat.bidelimited(p)) {
      zp;
    } else {
      ParenthesizedZ(zp);
    }
  | ParenthesizedZ(_)
  | InjZ(_, _, _) => zp
  | OpSeqZ(_, _, _) => ParenthesizedZ(zp)
  };

let rec set_err_status_t = (err: err_status, zp: t): t =>
  switch (zp) {
  | CursorP(cursor, p) =>
    let p = UHPat.set_err_status_t(err, p);
    CursorP(cursor, p);
  | ParenthesizedZ(zp1) => ParenthesizedZ(set_err_status_t(err, zp1))
  | InjZ(_, inj_side, zp1) => InjZ(err, inj_side, zp1)
  | OpSeqZ(skel, zp_n, surround) =>
    let (skel, zp_n, surround) =
      set_err_status_opseq(err, skel, zp_n, surround);
    OpSeqZ(skel, zp_n, surround);
  }
and set_err_status_opseq =
    (err: err_status, skel: UHPat.skel_t, zp_n: t, surround: opseq_surround)
    : (UHPat.skel_t, t, opseq_surround) =>
  switch (skel) {
  | Placeholder(m) =>
    if (m === OperatorSeq.surround_prefix_length(surround)) {
      let zp_n = set_err_status_t(err, zp_n);
      (skel, zp_n, surround);
    } else {
      switch (OperatorSeq.surround_nth(m, surround)) {
      | None => raise(SkelInconsistentWithOpSeq)
      | Some(p_m) =>
        let p_m = UHPat.set_err_status_t(err, p_m);
        switch (OperatorSeq.surround_update_nth(m, surround, p_m)) {
        | None => raise(SkelInconsistentWithOpSeq)
        | Some(surround) => (skel, zp_n, surround)
        };
      };
    }
  | BinOp(_, op, skel1, skel2) => (
      BinOp(err, op, skel1, skel2),
      zp_n,
      surround,
    )
  };

let rec make_t_inconsistent = (u_gen: MetaVarGen.t, zp: t): (t, MetaVarGen.t) =>
  switch (zp) {
  | CursorP(cursor, p) =>
    let (p, u_gen) = UHPat.make_t_inconsistent(u_gen, p);
    (CursorP(cursor, p), u_gen);
  | InjZ(InHole(TypeInconsistent, _), _, _) => (zp, u_gen)
  | InjZ(NotInHole | InHole(WrongLength, _), inj_side, zp1) =>
    let (u, u_gen) = MetaVarGen.next(u_gen);
    (InjZ(InHole(TypeInconsistent, u), inj_side, zp1), u_gen);
  | ParenthesizedZ(zp1) =>
    let (zp1, u_gen) = make_t_inconsistent(u_gen, zp1);
    (ParenthesizedZ(zp1), u_gen);
  | OpSeqZ(skel, zp_n, surround) =>
    let (skel, zp_n, surround, u_gen) =
      make_opseq_inconsistent(u_gen, skel, zp_n, surround);
    (OpSeqZ(skel, zp_n, surround), u_gen);
  }
and make_opseq_inconsistent =
    (
      u_gen: MetaVarGen.t,
      skel: UHPat.skel_t,
      zp_n: t,
      surround: opseq_surround,
    )
    : (UHPat.skel_t, t, opseq_surround, MetaVarGen.t) =>
  switch (skel) {
  | Placeholder(m) =>
    if (m === OperatorSeq.surround_prefix_length(surround)) {
      let (zp_n, u_gen) = make_t_inconsistent(u_gen, zp_n);
      (skel, zp_n, surround, u_gen);
    } else {
      switch (OperatorSeq.surround_nth(m, surround)) {
      | None => raise(SkelInconsistentWithOpSeq)
      | Some(p_m) =>
        let (p_m, u_gen) = UHPat.make_t_inconsistent(u_gen, p_m);
        switch (OperatorSeq.surround_update_nth(m, surround, p_m)) {
        | None => raise(SkelInconsistentWithOpSeq)
        | Some(surround) => (skel, zp_n, surround, u_gen)
        };
      };
    }
  | BinOp(InHole(TypeInconsistent, _), _, _, _) => (
      skel,
      zp_n,
      surround,
      u_gen,
    )
  | BinOp(NotInHole, op, skel1, skel2)
  | BinOp(InHole(WrongLength, _), op, skel1, skel2) =>
    let (u, u_gen) = MetaVarGen.next(u_gen);
    (
      BinOp(InHole(TypeInconsistent, u), op, skel1, skel2),
      zp_n,
      surround,
      u_gen,
    );
  };

let rec erase = (zp: t): UHPat.t =>
  switch (zp) {
  | CursorP(_, p) => p
  | InjZ(err_status, inj_side, zp1) => Inj(err_status, inj_side, erase(zp1))
  | ParenthesizedZ(zp) => Parenthesized(erase(zp))
  | OpSeqZ(skel, zp1, surround) =>
    let p1 = erase(zp1);
    OpSeq(skel, OperatorSeq.opseq_of_exp_and_surround(p1, surround));
  };

let rec is_before = (zp: t): bool =>
  switch (zp) {
  /* outer nodes - delimiter */
  | CursorP(cursor, EmptyHole(_))
  | CursorP(cursor, Wild(_))
  | CursorP(cursor, ListNil(_)) => cursor == OnDelim(0, Before)
  /* outer nodes - text */
  | CursorP(cursor, Var(_, _, _))
  | CursorP(cursor, NumLit(_, _))
  | CursorP(cursor, BoolLit(_, _)) => cursor == OnText(0)
  /* inner nodes */
  | CursorP(cursor, Inj(_, _, _))
  | CursorP(cursor, Parenthesized(_)) => cursor == OnDelim(0, Before)
  | CursorP(_, OpSeq(_, _)) => false
  /* zipper cases */
  | InjZ(_, _, _) => false
  | ParenthesizedZ(_) => false
  | OpSeqZ(_, zp1, EmptyPrefix(_)) => is_before(zp1)
  | OpSeqZ(_, _, _) => false
  };

let rec is_after = (zp: t): bool =>
  switch (zp) {
  /* outer nodes - delimiter */
  | CursorP(cursor, EmptyHole(_))
  | CursorP(cursor, Wild(_))
  | CursorP(cursor, ListNil(_)) => cursor == OnDelim(0, After)
  /* outer nodes - text */
  | CursorP(cursor, Var(_, _, x)) => cursor == OnText(Var.length(x))
  | CursorP(cursor, NumLit(_, n)) => cursor == OnText(num_digits(n))
  | CursorP(cursor, BoolLit(_, b)) => cursor == OnText(b ? 4 : 5)
  /* inner nodes */
  | CursorP(cursor, Inj(_, _, _))
  | CursorP(cursor, Parenthesized(_)) => cursor == OnDelim(1, After)
  | CursorP(_, OpSeq(_, _)) => false
  /* zipper cases */
  | InjZ(_, _, _) => false
  | ParenthesizedZ(_) => false
  | OpSeqZ(_, zp1, EmptySuffix(_)) => is_after(zp1)
  | OpSeqZ(_, _, _) => false
  };

let rec place_before = (p: UHPat.t): t =>
  switch (p) {
  /* outer nodes - delimiter */
  | EmptyHole(_)
  | Wild(_)
  | ListNil(_) => CursorP(OnDelim(0, Before), p)
  /* outer nodes - text */
  | Var(_, _, _)
  | NumLit(_, _)
  | BoolLit(_, _) => CursorP(OnText(0), p)
  /* inner nodes */
  | Inj(_, _, _)
  | Parenthesized(_) => CursorP(OnDelim(0, Before), p)
  | OpSeq(skel, seq) =>
    let (p0, suffix) = OperatorSeq.split0(seq);
    let surround = OperatorSeq.EmptyPrefix(suffix);
    let zp0 = place_before(p0);
    OpSeqZ(skel, zp0, surround);
  };

let rec place_after = (p: UHPat.t): t =>
  switch (p) {
  /* outer nodes - delimiter */
  | EmptyHole(_)
  | Wild(_)
  | ListNil(_) => CursorP(OnDelim(0, After), p)
  /* outer nodes - text */
  | Var(_, _, x) => CursorP(OnText(Var.length(x)), p)
  | NumLit(_, n) => CursorP(OnText(num_digits(n)), p)
  | BoolLit(_, b) => CursorP(OnText(b ? 4 : 5), p)
  /* inner nodes */
  | Inj(_, _, _) => CursorP(OnDelim(1, After), p)
  | Parenthesized(_) => CursorP(OnDelim(1, After), p)
  | OpSeq(skel, seq) =>
    let (p0, prefix) = OperatorSeq.split_tail(seq);
    let surround = OperatorSeq.EmptySuffix(prefix);
    let zp0 = place_after(p0);
    OpSeqZ(skel, zp0, surround);
  };

let place_cursor = (cursor: cursor_position, p: UHPat.t): option(t) =>
  is_valid_cursor(cursor, p) ? Some(CursorP(cursor, p)) : None;

/* helper function for constructing a new empty hole */
let new_EmptyHole = (u_gen: MetaVarGen.t): (t, MetaVarGen.t) => {
  let (hole, u_gen) = UHPat.new_EmptyHole(u_gen);
  (place_before(hole), u_gen);
};

let is_inconsistent = (zp: t): bool => UHPat.is_inconsistent(erase(zp));

let rec cursor_on_opseq = (zp: t): bool =>
  switch (zp) {
  | CursorP(_, OpSeq(_, _)) => true
  | CursorP(_, _) => false
  | ParenthesizedZ(zp) => cursor_on_opseq(zp)
  | OpSeqZ(_, zp, _) => cursor_on_opseq(zp)
  | InjZ(_, _, zp) => cursor_on_opseq(zp)
  };

let node_positions = (p: UHPat.t): list(node_position) =>
  switch (p) {
  | EmptyHole(_)
  | Wild(_)
  | Var(_, _, _)
  | NumLit(_, _)
  | BoolLit(_, _)
  | ListNil(_) => node_positions(valid_cursors(p))
  | Parenthesized(_)
  | Inj(_) =>
    node_positions(delim_cursors_k(0))
    @ [Deeper(0)]
    @ node_positions(delim_cursors_k(1))
  | OpSeq(_, seq) =>
    range(OperatorSeq.seq_length(seq))
    |> List.fold_left(
         (lstSoFar, i) =>
           switch (lstSoFar) {
           | [] => [Deeper(i)]
           | [_, ..._] =>
             lstSoFar @ node_positions(delim_cursors_k(i)) @ [Deeper(i)]
           },
         [],
       )
  };

let node_position_of_t = (zp: t): node_position =>
  switch (zp) {
  | CursorP(cursor, _) => On(cursor)
  | ParenthesizedZ(_) => Deeper(0)
  | InjZ(_, _, _) => Deeper(0)
  | OpSeqZ(_, _, surround) =>
    Deeper(OperatorSeq.surround_prefix_length(surround))
  };

let rec cursor_node_type = (zp: t): node_type =>
  switch (zp) {
  /* outer nodes */
  | CursorP(_, EmptyHole(_))
  | CursorP(_, Wild(_))
  | CursorP(_, Var(_, _, _))
  | CursorP(_, NumLit(_, _))
  | CursorP(_, BoolLit(_, _))
  | CursorP(_, ListNil(_)) => Outer
  /* inner nodes */
  | CursorP(_, Parenthesized(_))
  | CursorP(_, OpSeq(_, _))
  | CursorP(_, Inj(_, _, _)) => Inner
  /* zipper */
  | ParenthesizedZ(zp1) => cursor_node_type(zp1)
  | OpSeqZ(_, zp1, _) => cursor_node_type(zp1)
  | InjZ(_, _, zp1) => cursor_node_type(zp1)
  };

let rec diff_is_just_cursor_movement_within_node = (zp1, zp2) =>
  switch (zp1, zp2) {
  | (CursorP(_, p1), CursorP(_, p2)) => p1 == p2
  | (ParenthesizedZ(zbody1), ParenthesizedZ(zbody2)) =>
    diff_is_just_cursor_movement_within_node(zbody1, zbody2)
  | (OpSeqZ(skel1, ztm1, surround1), OpSeqZ(skel2, ztm2, surround2)) =>
    skel1 == skel2
    && diff_is_just_cursor_movement_within_node(ztm1, ztm2)
    && surround1 == surround2
  | (InjZ(err_status1, side1, zbody1), InjZ(err_status2, side2, zbody2)) =>
    err_status1 == err_status2
    && side1 == side2
    && diff_is_just_cursor_movement_within_node(zbody1, zbody2)
  | (_, _) => false
  };

let rec move_cursor_left = (zp: t): option(t) =>
  switch (zp) {
  | _ when is_before(zp) => None
  | CursorP(OnText(j), p) => Some(CursorP(OnText(j - 1), p))
  | CursorP(OnDelim(k, After), p) => Some(CursorP(OnDelim(k, Before), p))
  | CursorP(OnDelim(_, Before), EmptyHole(_) | Wild(_) | ListNil(_)) => None
  | CursorP(OnDelim(_k, Before), Parenthesized(p1)) =>
    // _k == 1
    Some(ParenthesizedZ(place_after(p1)))
  | CursorP(OnDelim(_k, Before), Inj(err_status, side, p1)) =>
    // _k == 1
    Some(InjZ(err_status, side, place_after(p1)))
  | CursorP(OnDelim(k, Before), OpSeq(skel, seq)) =>
    switch (seq |> OperatorSeq.split(k - 1)) {
    | None => None // should never happen
    | Some((p1, surround)) => Some(OpSeqZ(skel, place_after(p1), surround))
    }
  | CursorP(OnDelim(_, _), Var(_, _, _) | BoolLit(_, _) | NumLit(_, _)) =>
    // invalid cursor position
    None
  | ParenthesizedZ(zp1) =>
    switch (move_cursor_left(zp1)) {
    | Some(zp1) => Some(ParenthesizedZ(zp1))
    | None => Some(CursorP(OnDelim(0, After), Parenthesized(erase(zp1))))
    }
  | InjZ(err_status, side, zp1) =>
    switch (move_cursor_left(zp1)) {
    | Some(zp1) => Some(InjZ(err_status, side, zp1))
    | None =>
      Some(CursorP(OnDelim(0, After), Inj(err_status, side, erase(zp1))))
    }
  | OpSeqZ(skel, zp1, surround) =>
    switch (move_cursor_left(zp1)) {
    | Some(zp1) => Some(OpSeqZ(skel, zp1, surround))
    | None =>
      switch (surround) {
      | EmptyPrefix(_) => None
      | EmptySuffix(ExpPrefix(_, Space) | SeqPrefix(_, Space))
      | BothNonEmpty(ExpPrefix(_, Space) | SeqPrefix(_, Space), _) =>
        let k = OperatorSeq.surround_prefix_length(surround);
        let seq =
          OperatorSeq.opseq_of_exp_and_surround(erase(zp1), surround);
        switch (seq |> OperatorSeq.split(k - 1)) {
        | None => None // should never happen
        | Some((p1, surround)) =>
          Some(OpSeqZ(skel, place_after(p1), surround))
        };
      | _ =>
        let k = OperatorSeq.surround_prefix_length(surround);
        let seq =
          OperatorSeq.opseq_of_exp_and_surround(erase(zp1), surround);
        Some(CursorP(OnDelim(k, After), OpSeq(skel, seq)));
      }
    }
  };

let rec move_cursor_right = (zp: t): option(t) =>
  switch (zp) {
  | _ when is_after(zp) => None
  | CursorP(OnText(j), p) => Some(CursorP(OnText(j + 1), p))
  | CursorP(OnDelim(k, Before), p) => Some(CursorP(OnDelim(k, After), p))
  | CursorP(OnDelim(_, After), EmptyHole(_) | Wild(_) | ListNil(_)) => None
  | CursorP(OnDelim(_k, After), Parenthesized(p1)) =>
    // _k == 0
    Some(ParenthesizedZ(place_before(p1)))
  | CursorP(OnDelim(_k, After), Inj(err_status, side, p1)) =>
    // _k == 0
    Some(InjZ(err_status, side, place_before(p1)))
  | CursorP(OnDelim(k, After), OpSeq(skel, seq)) =>
    switch (seq |> OperatorSeq.split(k)) {
    | None => None // should never happen
    | Some((p1, surround)) =>
      Some(OpSeqZ(skel, place_before(p1), surround))
    }
  | CursorP(OnDelim(_, _), Var(_, _, _) | BoolLit(_, _) | NumLit(_, _)) =>
    // invalid cursor position
    None
  | ParenthesizedZ(zp1) =>
    switch (move_cursor_right(zp1)) {
    | Some(zp1) => Some(ParenthesizedZ(zp1))
    | None => Some(CursorP(OnDelim(1, Before), Parenthesized(erase(zp1))))
    }
  | InjZ(err_status, side, zp1) =>
    switch (move_cursor_right(zp1)) {
    | Some(zp1) => Some(InjZ(err_status, side, zp1))
    | None =>
      Some(CursorP(OnDelim(1, Before), Inj(err_status, side, erase(zp1))))
    }
  | OpSeqZ(skel, zp1, surround) =>
    switch (move_cursor_right(zp1)) {
    | Some(zp1) => Some(OpSeqZ(skel, zp1, surround))
    | None =>
      switch (surround) {
      | EmptySuffix(_) => None
      | EmptyPrefix(ExpSuffix(Space, _) | SeqSuffix(Space, _))
      | BothNonEmpty(_, ExpSuffix(Space, _) | SeqSuffix(Space, _)) =>
        let k = OperatorSeq.surround_prefix_length(surround);
        let seq =
          OperatorSeq.opseq_of_exp_and_surround(erase(zp1), surround);
        switch (seq |> OperatorSeq.split(k + 1)) {
        | None => None // should never happen
        | Some((p1, surround)) =>
          Some(OpSeqZ(skel, place_before(p1), surround))
        };
      | _ =>
        let k = OperatorSeq.surround_prefix_length(surround);
        let seq =
          OperatorSeq.opseq_of_exp_and_surround(erase(zp1), surround);
        Some(CursorP(OnDelim(k + 1, Before), OpSeq(skel, seq)));
      }
    }
  };
