/**
 * Tree-sitter grammar for SPIP template language.
 *
 * SPIP is a French CMS whose templates (.html files) mix HTML with
 * SPIP-specific constructs: loops (boucles), tags (balises), filters,
 * includes, multilingual blocks, and conditional brackets.
 *
 * This grammar recognises only the SPIP constructs and treats everything
 * else as `content` nodes.  HTML is then injected into those content
 * nodes via injections.scm in the Zed extension.
 *
 * IMPORTANT: All regex patterns use + (not *) to prevent empty-string
 * matches, which cause infinite loops in tree-sitter WASM.
 */

module.exports = grammar({
  name: "spip",

  externals: ($) => [
    $._content_char,
    $._spip_ws,
  ],

  extras: (_) => [],

  rules: {
    template: ($) =>
      repeat(
        choice(
          $.comment,
          $.loop_open,
          $.loop_close,
          $.loop_conditional_open,
          $.loop_conditional_close,
          $.loop_alternative,
          $.include_tag,
          $.multi_block,
          $.translation,
          $.balise,
          $.balise_shorthand,
          $.conditional_open,
          $.conditional_close,
          $.content,
        ),
      ),

    // ── Comments ──────────────────────────────────────────────
    // [(#REM) some comment text ]
    // Can span multiple lines: [(#REM)\n  multi-line comment\n]
    comment: (_) => seq("[(#REM)", optional(/[^\]]+/), "]"),

    // ── Loops (Boucles) ──────────────────────────────────────
    // <BOUCLE_name(TYPE){criteria}{sort}>
    // Criteria blocks may be separated by spaces or newlines:
    //   <BOUCLE_a(ARTICLES){id_rubrique !IN 3} {par date}{inverse}>
    //   <BOUCLE_boutons(DATA)
    //     {source tableau, #GET{boutons}}
    //     {cle!=outils_rapides}
    //   >
    loop_open: ($) =>
      seq(
        "<BOUCLE_",
        field("name", $.loop_name),
        "(",
        field("type", $.loop_type),
        ")",
        repeat(seq(optional($._spip_ws), $.criteria)),
        optional($._spip_ws),
        ">",
      ),

    // </BOUCLE_name>
    loop_close: ($) =>
      seq("</BOUCLE_", field("name", $.loop_name), ">"),

    // <B_name>
    loop_conditional_open: ($) =>
      seq("<B_", field("name", $.loop_name), ">"),

    // </B_name>
    loop_conditional_close: ($) =>
      seq("</B_", field("name", $.loop_name), ">"),

    // <//B_name>
    loop_alternative: ($) =>
      seq("<//B_", field("name", $.loop_name), ">"),

    // Loop names can start with digits: <BOUCLE_10recents(...)>
    loop_name: (_) => /[a-zA-Z0-9_]+/,
    // Type can be uppercase (ARTICLES) or a parent loop ref (BOUCLE_rubriques)
    loop_type: (_) => /[A-Z][A-Za-z0-9_]*/,

    // {criteria_content}  — supports nested braces like {si #ENV{x}}
    criteria: ($) =>
      seq("{", field("value", $.criteria_value), "}"),

    // Criteria value: everything inside { }, with support for nested
    // braces up to 3 levels deep.
    criteria_value: ($) =>
      repeat1(choice(
        /[^{}]+/,
        seq("{", optional($._nested_brace_content), "}"),
      )),

    // Nested brace content — supports 2+ levels deep.
    // Uses repeat1 and /[^{}]+/ (with +) to never match empty string.
    _nested_brace_content: ($) =>
      repeat1(choice(
        /[^{}]+/,
        seq("{", optional($._deep_brace_content), "}"),
      )),

    _deep_brace_content: (_) => /[^{}]+/,

    // ── Include ──────────────────────────────────────────────
    // <INCLURE{fond=header}{env}{home=oui} />
    // Also supports legacy syntax: <INCLURE{fond=header}>
    include_tag: ($) =>
      seq(
        "<INCLURE",
        repeat1(seq(optional($._spip_ws), $.include_param_block)),
        optional($._spip_ws),
        choice("/>", ">"),
      ),

    include_param_block: ($) =>
      seq("{", field("params", $.include_params), "}"),

    // Include params: supports nested braces up to 3 levels deep.
    include_params: ($) =>
      repeat1(choice(
        /[^{}]+/,
        seq("{", optional($._nested_brace_content), "}"),
      )),

    // ── Multilingual ─────────────────────────────────────────
    // Two forms:
    //   <multi>[fr]texte français[en]english text</multi>
    //   <multi>{en}Edit{fr}Modifier</multi>
    multi_block: ($) =>
      seq(
        "<multi>",
        repeat(
          choice(
            $.lang_code,
            $.lang_code_brace,
            $.multi_text,
          ),
        ),
        "</multi>",
      ),

    lang_code: (_) => /\[[a-z]{2}\]/,
    lang_code_brace: (_) => /\{[a-z]{2}\}/,
    // Match any text inside <multi> except delimiters [xx], {xx} and </multi>
    // We allow < when not followed by / (which would be </multi>)
    // and </ when not followed by m (which avoids </multi>)
    multi_text: (_) => /([^\[{<]|<[^\/\[{<]|<\/[^m])+/,

    // ── Translation strings ──────────────────────────────────
    // <:module:string:> or <:string:> or <:string|filter:>
    translation: (_) =>
      seq(
        "<:",
        /[a-zA-Z_][a-zA-Z0-9_]*(:[a-zA-Z_][a-zA-Z0-9_]*)*(\|[a-zA-Z_][a-zA-Z0-9_]*)*/,
        ":>",
      ),

    // ── Balises (tags) ───────────────────────────────────────
    // (#TAG_NAME), (#TAG_NAME{p}), (#TAG_NAME|filter{param})
    // (#TAG_NAME**), (#TAG_NAME*)
    // Can span multiple lines:
    //   (#MODELE{picture}
    //     {fichier=#LOGO}
    //     {traitement=focus})
    balise: ($) =>
      seq(
        "(#",
        optional(field("namespace", $.balise_namespace)),
        field("name", $.balise_name),
        optional(/\*{1,2}/),  // #TAG* or #TAG**
        repeat(seq(optional($._spip_ws), $.balise_params)),
        repeat(seq(optional($._spip_ws), $.filter)),
        optional($._spip_ws),
        ")",
      ),

    balise_namespace: (_) => /_[a-zA-Z]+:/,
    balise_name: (_) => /[A-Z][A-Z0-9_]*/,

    balise_params: ($) =>
      seq("{", field("value", optional($.param_content)), "}"),

    // #TAG_NAME or #TAG_NAME* or #TAG_NAME**
    // Shorthand balises do NOT parse {params} — use (#TAG{param}) for that.
    // This is because { cannot be reliably blocked from content only after
    // a shorthand (it would break CSS/JS where { is common).
    balise_shorthand: ($) =>
      seq(
        "#",
        field("name", $.balise_name),
        optional(/\*{1,2}/),
      ),

    // ── Filters ──────────────────────────────────────────────
    // |filter_name or |filter_name{param}
    // Operator filters: |=={value}, |!={value}, |>{value}, |<{value}
    // Special: |?{yes,no}
    filter: ($) =>
      seq(
        "|",
        field("name", $.filter_name),
        optional($.filter_params),
      ),

    // Filter names include comparison operators and special chars
    filter_name: (_) => /[a-zA-Z_!=<>?*][a-zA-Z0-9_!=<>?*]*/,

    filter_params: ($) =>
      seq("{", field("value", optional($.param_content)), "}"),

    // Shared rule for parameter content inside { }
    // Handles nested braces up to 3 levels deep for expressions like:
    //   {#ARRAY{a,b}}
    //   {#LISTE{300,360,400}}
    //   {#ENV{nombre_liens_max,#CONST{_PAGINATION_NOMBRE_LIENS_MAX}}}
    param_content: ($) =>
      repeat1(choice(
        /[^{}]+/,
        seq("{", optional($._nested_brace_content), "}"),
      )),

    // ── Conditional brackets ─────────────────────────────────
    // [ ... ] around balises for conditional display
    conditional_open: (_) => "[",
    conditional_close: (_) => "]",

    // ── Content (everything else = HTML) ─────────────────────
    // The external scanner emits _content_char for characters that
    // are not part of any SPIP construct.
    content: ($) => prec.right(repeat1($._content_char)),
  },
});
