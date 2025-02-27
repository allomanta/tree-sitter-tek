/**
 * @file The tek markup language used for Teksty
 * @author Allomanta
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: 'tek',

  extras: ($) => ['\r', $.comment, alias(/[^\S\n]+/, $.whitespace), '$$', '//'],

  externals: ($) => [
    $._start_section_tag_name,
    $._end_section_tag_name,
    $.erroneous_end_tag,
    $.literal,
    $.bracket_open,
    $.bracket_close,
    $.eof,
  ],

  rules: {
    source_file: ($) => $.content,

    content: ($) => repeat1($.block),

    block: ($) => choice($.section, $.variable_assignment, $.block_equations, $.line_end, $.paragraph),

    section: ($) => choice($.paired_section, $.bracketed_section),

    bracketed_section: ($) =>
      seq($.start_section_tag, optional($.label), $.bracket_open, $.line_end, optional($.content), $.bracket_close),

    paired_section: ($) =>
      seq($.start_section_tag, optional($.label), $.line_end, optional($.content), $.end_section_tag),

    label: ($) => choice($.identifier, $.literal),

    start_section_tag: ($) => seq('\\', $._start_section_tag_name),

    end_section_tag: ($) => seq('/', $._end_section_tag_name),

    inline_section: ($) => seq($.start_section_tag, $.bracket_open, repeat($.line_content), $.bracket_close),

    variable_assignment: ($) => prec(2, seq($.variable, '=', repeat1($.line_content), $.line_end)),

    inline_equations: ($) => seq($.eq_start, optional($.equation), $.eq_end),

    eq_start: ($) => '\\[',

    eq_end: ($) => token(choice(']/', '/]')),

    block_equations: ($) => seq($.eq_start, repeat1(seq(optional($.equation), $.line_end)), $.eq_end),

    equation: ($) =>
      repeat1(
        choice(
          $.variable,
          $.fraction,
          $.superscript,
          $.subscript,
          $.inline_section,
          $.eq_group,
          $.eq_text,
          $.literal,
          '=',
          '|',
          '||',
          '^^',
          '__',
        ),
      ),

    fraction: ($) => seq($.eq_group, '/', $.eq_group),

    eq_group: ($) => seq($.bracket_open, optional($.equation), $.bracket_close),

    text_group: ($) => seq($.bracket_open, repeat(seq(optional($.line_end), $.line_content)), $.bracket_close),

    superscript: ($) => seq('^', $.eq_group),

    subscript: ($) => seq('_', $.eq_group),

    paragraph: ($) => prec.right(seq($.line_content, repeat(seq(optional($.line_end), $.line_content)), $.line_end)),

    line_content: ($) => choice($.inline_section, $.inline_equations, $.text_group, $.literal, $.variable, $.text, '='),

    text: (_) => /[^\s\\$(){}\[\]`=/][^\n\\$(){}\[\]`]*/,
    eq_text: (_) => /[^\s\\$(){}\[\]`/|][^\n\\$(){}\[\]`/|]*/,

    variable: ($) => seq('$', $.identifier),

    identifier: (_) => /[\p{L}\p{N}\p{Mn}\p{Mc}\p{Pc}-]+/u,

    line_end: (_) => '\n',

    comment: ($) => seq('{*', alias(repeat(/.|\n/), $.comment_text), '*}'),
  },
});
