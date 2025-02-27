(ERROR) @error

(comment) @comment

[(start_section_tag) (end_section_tag)] @tag

(variable) @variable.parameter

(fraction) @operator

(literal) @markup.raw


(label) @attribute
(label (literal)) @attribute

[(block_equations) (inline_equations)] @constant.numeric
[(eq_start) (eq_end)] @keyword.function

[(bracket_close) (bracket_open)] @punctuation.bracket 
