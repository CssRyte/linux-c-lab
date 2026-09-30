import ply.lex as lex


# ============================================================
# Lab 4: Building a Multi-State Lexer with PLY
# ============================================================


# ============================================================
# LEXER STATES
# ============================================================

states = (
    ('string', 'exclusive'),
    ('comment', 'exclusive'),
)


# ============================================================
# RESERVED KEYWORDS
# ============================================================

reserved = {
    'let': 'LET',
    'fn': 'FN',
    'if': 'IF',
    'else': 'ELSE',
    'type': 'TYPE',
    'import': 'IMPORT',
}


# ============================================================
# TOKENS
# ============================================================

tokens = [
    # Identifiers and literals
    'ID',
    'INT',
    'HEX',
    'BIN',
    'FLOAT',

    # Multi-character operators
    'EQ',
    'NE',
    'LE',
    'GE',
    'ARROW',
    'AND',
    'OR',

    # String interpolation
    'STRING_START',
    'STRING_CONTENT',
    'STRING_END',
    'INTERP_START',

    # Operators
    'PLUS',
    'MINUS',
    'TIMES',
    'DIVIDE',
    'ASSIGN',
    'LT',
    'GT',

    # Delimiters
    'LPAREN',
    'RPAREN',
    'LBRACE',
    'RBRACE',
    'LBRACKET',
    'RBRACKET',
    'COMMA',
    'COLON',
    'SEMICOLON',

    # Challenge tokens
    'INDENT',
    'DEDENT',
    'ERROR_TOKEN',

] + list(reserved.values())


# ============================================================
# MULTI-CHARACTER OPERATORS
# ============================================================

t_EQ = r'=='
t_NE = r'!='
t_LE = r'<='
t_GE = r'>='
t_ARROW = r'->'
t_AND = r'&&'
t_OR = r'\|\|'


# ============================================================
# SINGLE-CHARACTER OPERATORS
# ============================================================

t_PLUS = r'\+'
t_MINUS = r'-'
t_TIMES = r'\*'
t_DIVIDE = r'/'
t_ASSIGN = r'='
t_LT = r'<'
t_GT = r'>'


# ============================================================
# DELIMITERS
# ============================================================

t_LPAREN = r'\('
t_RPAREN = r'\)'
t_LBRACE = r'\{'
t_LBRACKET = r'\['
t_RBRACKET = r'\]'
t_COMMA = r','
t_COLON = r':'
t_SEMICOLON = r';'


# Ignore ordinary spaces and tabs
t_ignore = ' \t'


# ============================================================
# SINGLE-LINE COMMENTS
# ============================================================

def t_singleline_comment(t):
    r'//[^\n]*'
    pass


# ============================================================
# NESTED MULTI-LINE COMMENTS
# ============================================================

def t_multiline_comment(t):
    r'/\*'

    t.lexer.comment_depth = 1
    t.lexer.begin('comment')


def t_comment_open(t):
    r'/\*'

    t.lexer.comment_depth += 1


def t_comment_close(t):
    r'\*/'

    t.lexer.comment_depth -= 1

    if t.lexer.comment_depth == 0:
        t.lexer.begin('INITIAL')


def t_comment_newline(t):
    r'\n+'

    t.lexer.lineno += len(t.value)


def t_comment_content(t):
    r'[^*/\n]+'
    pass


def t_comment_other(t):
    r'[*/]'
    pass


t_comment_ignore = ''


def t_comment_error(t):
    t.lexer.skip(1)


# ============================================================
# CHALLENGE 3:
# FLOATING-POINT PRECISION VALIDATION
#
# Examples:
# 1.0f32
# 1.0f64
# 1.0e20f32
# 1.0e200f64
# ============================================================

def t_FLOAT(t):
    r'((\d+\.\d*|\.\d+)([eE][+-]?\d+)?|\d+[eE][+-]?\d+)(f32|f64)?'

    original = t.value

    # Determine requested precision
    if original.endswith('f32'):

        precision = 'f32'
        number_text = original[:-3]

        # Approximate IEEE-754 decimal exponent range
        minimum_exponent = -45
        maximum_exponent = 38

    elif original.endswith('f64'):

        precision = 'f64'
        number_text = original[:-3]

        minimum_exponent = -324
        maximum_exponent = 308

    else:

        # Treat unsuffixed float as normal double precision
        precision = 'f64'
        number_text = original

        minimum_exponent = -324
        maximum_exponent = 308

    # Validate exponent when scientific notation is present
    if 'e' in number_text.lower():

        exponent_text = number_text.lower().split('e')[1]

        exponent = int(exponent_text)

        if (
            exponent < minimum_exponent
            or exponent > maximum_exponent
        ):

            print(
                f"Floating-point lexical error "
                f"at line {t.lexer.lineno}: "
                f"{original} exceeds {precision} exponent bounds"
            )

            t.type = 'ERROR_TOKEN'
            t.value = original

            return t

    t.value = {
        'value': float(number_text),
        'precision': precision
    }

    return t


# ============================================================
# HEX
# ============================================================

def t_HEX(t):
    r'0[xX][0-9a-fA-F]+'

    t.value = int(t.value, 16)

    return t


# ============================================================
# BINARY
# ============================================================

def t_BIN(t):
    r'0[bB][01]+'

    t.value = int(t.value, 2)

    return t


# ============================================================
# INTEGER
# ============================================================

def t_INT(t):
    r'\d+'

    t.value = int(t.value)

    return t


# ============================================================
# IDENTIFIERS / KEYWORDS
# ============================================================

def t_ID(t):
    r'[A-Za-z_][A-Za-z0-9_]*'

    t.type = reserved.get(t.value, 'ID')

    return t


# ============================================================
# STRING INTERPOLATION CLOSING BRACE
# ============================================================

def t_RBRACE(t):
    r'\}'

    if getattr(t.lexer, 'in_interpolation', False):

        t.lexer.in_interpolation = False
        t.lexer.begin('string')

    return t


# ============================================================
# STRING STATE
# ============================================================

def t_STRING_START(t):
    r'"'

    t.lexer.begin('string')

    return t


def t_string_INTERP_START(t):
    r'\$\{'

    t.lexer.in_interpolation = True

    # Return to normal lexer rules for expression
    t.lexer.begin('INITIAL')

    return t


def t_string_STRING_END(t):
    r'"'

    t.type = 'STRING_END'

    t.lexer.begin('INITIAL')

    return t


def t_string_STRING_CONTENT(t):
    r'([^"\\$]|\$(?!\{)|\\.)+'

    t.type = 'STRING_CONTENT'

    return t


t_string_ignore = ''


def t_string_newline(t):
    r'\n'

    t.lexer.lineno += 1

    t.type = 'STRING_CONTENT'

    return t


def t_string_error(t):

    print(
        f"String lexical error at line "
        f"{t.lexer.lineno}: "
        f"{t.value[0]!r}"
    )

    t.lexer.skip(1)


# ============================================================
# NEWLINES
# ============================================================

def t_newline(t):
    r'\n+'

    t.lexer.lineno += len(t.value)


# ============================================================
# CHALLENGE 2:
# ENHANCED ERROR CORRECTION
#
# Groups consecutive invalid characters into one ERROR_TOKEN.
# ============================================================

def t_error(t):

    source = t.lexer.lexdata

    start_position = t.lexpos

    end_position = start_position

    # Characters that can begin legitimate tokens
    valid_start_characters = (
        'abcdefghijklmnopqrstuvwxyz'
        'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
        '0123456789_'
        ' \t\r\n'
        '+-*/=<>!&|'
        '(){}[],:;"'
    )

    # Gather consecutive invalid characters
    while (
        end_position < len(source)
        and source[end_position] not in valid_start_characters
    ):

        end_position += 1

    # Always consume at least one character
    if end_position == start_position:
        end_position += 1

    bad_text = source[start_position:end_position]

    # Calculate starting column
    previous_newline = source.rfind(
        '\n',
        0,
        start_position
    )

    column = start_position - previous_newline

    print(
        f"Lexical error at line "
        f"{t.lexer.lineno}, "
        f"column {column}: "
        f"{bad_text!r}"
    )

    t.type = 'ERROR_TOKEN'
    t.value = bad_text

    # Skip the entire invalid sequence
    t.lexer.lexpos = end_position

    return t


# ============================================================
# BUILD LEXER
# ============================================================

lexer = lex.lex()


# ============================================================
# CHALLENGE 1:
# INDENTATION-BASED BLOCK SCOPING
#
# Maintains indentation levels using a stack.
# Produces INDENT and DEDENT tokens.
# ============================================================

def create_indent_token(
    token_type,
    line_number,
    position
):

    token = lex.LexToken()

    token.type = token_type
    token.value = token_type
    token.lineno = line_number
    token.lexpos = position

    return token


def tokenize_with_indentation(source):

    # Reset lexer
    lexer.lineno = 1
    lexer.begin('INITIAL')

    lexer.input(source)

    # Python-style indentation stack
    indentation_stack = [0]

    previous_line = None

    while True:

        token = lexer.token()

        if token is None:
            break

        # First visible token on a new line
        if token.lineno != previous_line:

            previous_line = token.lineno

            line_start = (
                source.rfind(
                    '\n',
                    0,
                    token.lexpos
                )
                + 1
            )

            prefix = source[
                line_start:token.lexpos
            ]

            # Only treat whitespace-only prefix as indentation
            if prefix.strip() == '':

                indentation = 0

                for character in prefix:

                    if character == ' ':
                        indentation += 1

                    elif character == '\t':
                        indentation += 4

                current_indent = (
                    indentation_stack[-1]
                )

                # New indentation level
                if indentation > current_indent:

                    indentation_stack.append(
                        indentation
                    )

                    yield create_indent_token(
                        'INDENT',
                        token.lineno,
                        token.lexpos
                    )

                # Return to one or more previous levels
                elif indentation < current_indent:

                    while (
                        len(indentation_stack) > 1
                        and indentation
                        < indentation_stack[-1]
                    ):

                        indentation_stack.pop()

                        yield create_indent_token(
                            'DEDENT',
                            token.lineno,
                            token.lexpos
                        )

        yield token

    # Close indentation levels at EOF
    while len(indentation_stack) > 1:

        indentation_stack.pop()

        yield create_indent_token(
            'DEDENT',
            lexer.lineno,
            len(source)
        )


# ============================================================
# TEST DRIVER
# ============================================================

if __name__ == '__main__':

    sample_code = r'''
import math;

let decimalValue = 42;
let hexValue = 0x1A3F;
let binaryValue = 0b1011;

let normalFloat = 3.1415;
let scientific = 6.022e23;

let scientific32 = 1.0e20f32;
let scientific64 = 1.0e200f64;

// This comment should be ignored

/*
    Outer comment

    /* Nested comment */

    Back in the outer comment
*/

if decimalValue >= 10
    let message = "Value is ${decimalValue}";
    let insideBlock = 25;
let finished = 1;

// Test grouped illegal characters
let broken = @#$ 50;

// Test floating-point exponent errors
let tooLarge32 = 1.0e50f32;
let tooLarge64 = 1.0e400f64;
'''

    print(
        f"{'TYPE':<15}"
        f"{'VALUE':<40}"
        f"{'LINE':<8}"
        f"POSITION"
    )

    print("-" * 85)

    for token in tokenize_with_indentation(
        sample_code
    ):

        print(
            f"{token.type:<15}"
            f"{str(token.value):<40}"
            f"{token.lineno:<8}"
            f"{token.lexpos}"
        )