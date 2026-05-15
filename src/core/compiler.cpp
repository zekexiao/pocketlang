/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_AMALGAMATED
#include "compiler.h"
#include "core.h"
#include "buffers.h"
#include "utils.h"
#include "vm.h"
#include "debug.h"
#endif

// The maximum number of locals or global (if compiling top level module)
// to lookup from the compiling context. Also it's limited by it's opcode
// which is using a single byte value to identify the local.
#define MAX_VARIABLES 256

// The maximum number of constant literal a module can contain. Also it's
// limited by it's opcode which is using a short value to identify.
#define MAX_CONSTANTS (1 << 16)

// The maximum number of upvaues a literal function can capture from it's
// enclosing function.
#define MAX_UPVALUES 256

// The maximum number of names that were used before defined. Its just the size
// of the Forward buffer of the compiler. Feel free to increase it if it
// require more.
#define MAX_FORWARD_NAMES 256

// Pocketlang support two types of interpolation.
//
//   1. Name interpolation       ex: "Hello $name!"
//   2. Expression interpolation ex: "Hello ${getName()}!"
//
// Consider a string: "a ${ b "c ${d}" } e" -- Here the depth of 'b' is 1 and
// the depth of 'd' is 2 and so on. The maximum depth an expression can go is
// defined as MAX_STR_INTERP_DEPTH below.
#define MAX_STR_INTERP_DEPTH 8

// The maximum address possible to jump. Similar limitation as above.
#define MAX_JUMP (1 << 16)

// Max number of break statement in a loop statement to patch.
#define MAX_BREAK_PATCH 256

/*****************************************************************************/
/* TOKENS                                                                    */
/*****************************************************************************/

typedef enum {

  TK_ERROR = 0,
  TK_EOF,
  TK_LINE,

  // symbols
  TK_DOT,        // .
  TK_DOTDOT,     // ..
  TK_COMMA,      // ,
  TK_COLLON,     // :
  TK_SEMICOLLON, // ;
  TK_HASH,       // #
  TK_LPARAN,     // (
  TK_RPARAN,     // )
  TK_LBRACKET,   // [
  TK_RBRACKET,   // ]
  TK_LBRACE,     // {
  TK_RBRACE,     // }
  TK_PERCENT,    // %

  TK_TILD,       // ~
  TK_AMP,        // &
  TK_PIPE,       // |
  TK_CARET,      // ^
  TK_ARROW,      // ->

  TK_PLUS,       // +
  TK_MINUS,      // -
  TK_STAR,       // *
  TK_FSLASH,     // /
  TK_STARSTAR,   // **
  TK_BSLASH,     // \.
  TK_EQ,         // =
  TK_GT,         // >
  TK_LT,         // <

  TK_EQEQ,       // ==
  TK_NOTEQ,      // !=
  TK_GTEQ,       // >=
  TK_LTEQ,       // <=

  TK_PLUSEQ,     // +=
  TK_MINUSEQ,    // -=
  TK_STAREQ,     // *=
  TK_DIVEQ,      // /=
  TK_MODEQ,      // %=
  TK_POWEQ,      // **=

  TK_ANDEQ,      // &=
  TK_OREQ,       // |=
  TK_XOREQ,      // ^=

  TK_SRIGHT,     // >>
  TK_SLEFT,      // <<

  TK_SRIGHTEQ,   // >>=
  TK_SLEFTEQ,    // <<=

  // Keywords.
  TK_CLASS,      // class
  TK_FROM,       // from
  TK_IMPORT,     // import
  TK_AS,         // as
  TK_DEF,        // def
  TK_NATIVE,     // native (C function declaration)
  TK_FN,         // function (literal function)
  TK_END,        // end

  TK_NULL,       // null
  TK_IN,         // in
  TK_IS,         // is
  TK_AND,        // and
  TK_OR,         // or
  TK_NOT,        // not / !
  TK_TRUE,       // true
  TK_FALSE,      // false
  TK_SELF,       // self
  TK_SUPER,      // super

  TK_DO,         // do
  TK_THEN,       // then
  TK_WHILE,      // while
  TK_FOR,        // for
  TK_IF,         // if
  TK_ELIF,       // elif
  TK_ELSE,       // else
  TK_BREAK,      // break
  TK_CONTINUE,   // continue
  TK_RETURN,     // return

  TK_NAME,       // identifier

  TK_NUMBER,     // number literal
  TK_STRING,     // string literal

  /* String interpolation
   *  "a ${b} c $d e"
   * tokenized as:
   *   TK_STR_INTERP  "a "
   *   TK_NAME        b
   *   TK_STR_INTERP  " c "
   *   TK_NAME        d
   *   TK_STRING     " e" */
   TK_STRING_INTERP,

} _TokenType;
// Winint.h has already defined TokenType which breaks amalgam build so I've
// change the name from TokenType to _TokenType.

typedef struct {
  _TokenType type;

  const char* start; //< Begining of the token in the source.
  int length;        //< Number of chars of the token.
  int line;          //< Line number of the token (1 based).
  Var value;         //< Literal value of the token.
} Token;

typedef struct {
  const char* identifier;
  int length;
  _TokenType tk_type;
} _Keyword;

// List of keywords mapped into their identifiers.
static _Keyword _keywords[] = {
  { "class",    5, TK_CLASS    },
  { "from",     4, TK_FROM     },
  { "import",   6, TK_IMPORT   },
  { "as",       2, TK_AS       },
  { "def",      3, TK_DEF      },
  { "native",   6, TK_NATIVE   },
  { "fn",       2, TK_FN       },
  { "end",      3, TK_END      },
  { "null",     4, TK_NULL     },
  { "in",       2, TK_IN       },
  { "is",       2, TK_IS       },
  { "and",      3, TK_AND      },
  { "or",       2, TK_OR       },
  { "not",      3, TK_NOT      },
  { "true",     4, TK_TRUE     },
  { "false",    5, TK_FALSE    },
  { "self",     4, TK_SELF     },
  { "super",    5, TK_SUPER    },
  { "do",       2, TK_DO       },
  { "then",     4, TK_THEN     },
  { "while",    5, TK_WHILE    },
  { "for",      3, TK_FOR      },
  { "if",       2, TK_IF       },
  { "elif",     4, TK_ELIF     },
  { "else",     4, TK_ELSE     },
  { "break",    5, TK_BREAK    },
  { "continue", 8, TK_CONTINUE },
  { "return",   6, TK_RETURN   },

  { NULL,       0, (_TokenType)(0) }, // Sentinel to mark the end of the array.
};

/*****************************************************************************/
/* COMPILER INTERNAL TYPES                                                   */
/*****************************************************************************/

// Precedence parsing references:
// http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/

typedef enum {
  PREC_NONE,
  PREC_LOWEST,
  PREC_LOGICAL_OR,    // or
  PREC_LOGICAL_AND,   // and
  PREC_EQUALITY,      // == !=
  PREC_TEST,          // in is
  PREC_COMPARISION,   // < > <= >=
  PREC_BITWISE_OR,    // |
  PREC_BITWISE_XOR,   // ^
  PREC_BITWISE_AND,   // &
  PREC_BITWISE_SHIFT, // << >>
  PREC_RANGE,         // ..
  PREC_TERM,          // + -
  PREC_FACTOR,        // * / %
  PREC_UNARY,         // - ! ~ not
  PREC_EXPONENT,      // **
  PREC_CALL,          // ()
  PREC_SUBSCRIPT,     // []
  PREC_ATTRIB,        // .index
  PREC_PRIMARY,
} Precedence;

typedef void (Compiler::*GrammarFn)();

typedef struct {
  GrammarFn prefix;
  GrammarFn infix;
  Precedence precedence;
} GrammarRule;

typedef enum {
  DEPTH_GLOBAL = -1, //< Global variables.
  DEPTH_LOCAL,       //< Local scope. Increase with inner scope.
} Depth;

typedef enum {
  FUNC_MAIN, // The body function of the script.
  FUNC_TOPLEVEL,
  FUNC_LITERAL,
  FUNC_METHOD,
  FUNC_CONSTRUCTOR,
} FuncType;

typedef struct {
  const char* name; //< Directly points into the source string.
  uint32_t length;  //< Length of the name.
  int depth;        //< The depth the local is defined in.
  bool is_upvalue;  //< Is this an upvalue for a nested function.
  int line;         //< The line variable declared for debugging.
} Local;

typedef struct sLoop {

  // Index of the loop's start instruction where the execution will jump
  // back to once it reach the loop end or continue used.
  int start;

  // Index of the jump out address instruction to patch it's value once done
  // compiling the loop.
  int exit_jump;

  // Array of address indexes to patch break address.
  int patches[MAX_BREAK_PATCH];
  int patch_count;

  // The outer loop of the current loop used to set and reset the compiler's
  // current loop context.
  struct sLoop* outer_loop;

  // Depth of the loop, required to pop all the locals in that loop when it
  // met a break/continue statement inside.
  int depth;

} Loop;

// ForwardName is used for globals that are accessed before defined inside
// a local scope.
// TODO: Since function and class global variables are initialized at the
//       compile time we can allow access to them at the global scope.
typedef struct sForwardName {

  // Index of the short instruction that has the value of the global's name
  // (in the names buffer of the module).
  int instruction;

  // The function where the name is used, and the instruction is belongs to.
  Fn* func;

  // Name token that was lexed for this name.
  Token tkname;

} ForwardName;

// This struct is used to keep track about the information of the upvaues for
// the current function to generate opcodes to capture them.
typedef struct sUpvalueInfo {

  // If it's true the extrenal local belongs to the immediate enclosing
  // function and the bellow [index] refering at the locals of that function.
  // If it's false the external local of the upvalue doesn't belongs to the
  // immediate enclosing function and the [index] will refering to the upvalues
  // array of the enclosing function.
  bool is_immediate;

  // Index of the upvalue's external local variable, in the local or upvalues
  // array of the enclosing function.
  int index;

} UpvalueInfo;

typedef struct sFunc {

  // Type of the current function.
  FuncType type;

  // Scope of the function. -2 for module body function, -1 for top level
  // function and literal functions will have the scope where it declared.
  int depth;

  Local locals[MAX_VARIABLES]; //< Variables in the current context.
  int local_count; //< Number of locals in [locals].

  UpvalueInfo upvalues[MAX_UPVALUES]; //< Upvalues in the current context.

  int stack_size;  //< Current size including locals ind temps.

  // The actual function pointer which is being compiled.
  Function* ptr;

  // If outer function of this function, for top level function the outer
  // function will be the module's body function.
  struct sFunc* outer_func;

} Func;

// A convenient macro to get the current function.
#define _FN (this->func->ptr->fn)

// The context of the parsing phase for the compiler.
class Parser {
public:
  Parser() = default;
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;
  Parser(Parser&&) = delete;
  Parser& operator=(Parser&&) = delete;

  // Parser need a reference of the PKVM to allocate strings (for string
  // literals in the source) and to report error if there is any.
  PKVM* vm;

  // The [source] and the [file_path] are pointers to an allocated string.
  // The parser doesn't keep references to that objects (to prevent them
  // from garbage collected). It's the compiler's responsibility to keep the
  // strings alive alive as long as the parser is alive.

  const char* source;       //< Currently compiled source.
  const char* file_path;    //< Path of the module (for reporting errors).
  const char* token_start;  //< Start of the currently parsed token.
  const char* current_char; //< Current char position in the source.
  int current_line;         //< Line number of the current char.
  Token previous, current, next; //< Currently parsed tokens.

  // The current depth of the string interpolation. 0 means we're not inside
  // an interpolated string.
  int si_depth;

  // If we're parsing an interpolated string and found a TK_RBRACE (ie. '}')
  // we need to know if that's belongs to the expression we're parsing, or the
  // end of the current interpolation.
  //
  // To achieve that We need to keep track of the number of open brace at the
  // current depth. If we don't have any open brace then the TK_RBRACE token
  // is consumed to end the interpolation.
  //
  // If we're inside an interpolated string (ie. si_depth > 0)
  // si_open_brace[si_depth - 1] will return the number of open brace at the
  // current depth.
  int si_open_brace[MAX_STR_INTERP_DEPTH];

  // Since we're supporting both quotes (single and double), we need to keep
  // track of the qoute the interpolation is surrounded by to properly
  // terminate the string.
  // here si_quote[si_depth - 1] will return the surrunded quote of the
  // expression at current depth.
  char si_quote[MAX_STR_INTERP_DEPTH];

  // When we're parsing a name interpolated string (ie. "Hello $name!") we
  // have to keep track of where the name ends to start the interpolation
  // from there. The below value [si_name_end] will be NULL if we're not
  // parsing a name interpolated string, otherwise it'll points to the end of
  // the name.
  //
  // Also we're using [si_name_quote] to store the quote of the string to
  // properly terminate.
  const char* si_name_end;
  char si_name_quote;

  // An array of implicitly forward declared names, which will be resolved once
  // the module is completely compiled.
  ForwardName forwards[MAX_FORWARD_NAMES];
  int forwards_count;

  // A syntax sugar to skip call parentheses. Like lua support for number of
  // literals. We're doing it for literal functions for now. It'll be set to
  // true before exprCall to indicate that the call paran should be skipped.
  bool optional_call_paran;

  bool repl_mode;
  bool parsing_class;
  bool need_more_lines; //< True if we need more lines in REPL mode.

  // [has_errors] is for all kinds of errors, If it's set we don't terminate
  // the compilation since we can cascade more errors by continuing. But
  // [has_syntax_error] will set to true if we encounter one and this will
  // terminatie the compilation.
  bool has_syntax_error;
  bool has_errors;

  // Parser methods
  void init(PKVM* vm, Compiler* compiler,
            const char* source, const char* path);
  void reportError(Token tk, const char* fmt, va_list args);
  Token makeErrToken();
  char peekChar();
  char peekNextChar();
  char eatChar();
  void setNextValueToken(_TokenType type, Var value);
  void setNextToken(_TokenType type);
  bool matchChar(char c);
  void eatName();
  void skipLineComment();
  void setNextTwoCharToken(char c, _TokenType one, _TokenType two);
};

// Result type for an identifier definition.
typedef enum {
  NAME_NOT_DEFINED,
  NAME_LOCAL_VAR,  //< Including parameter.
  NAME_UPVALUE,    //< Local to an enclosing function.
  NAME_GLOBAL_VAR,
  NAME_BUILTIN_FN, //< Native builtin function.
  NAME_BUILTIN_TY, //< Builtin primitive type classes.
} NameDefnType;

// Identifier search result.
typedef struct {

  NameDefnType type;

  // Index in the variable/function buffer/array.
  int index;

  // The line it declared.
  int line;

} NameSearchResult;

typedef enum {
  BLOCK_FUNC,
  BLOCK_LOOP,
  BLOCK_IF,
  BLOCK_ELSE,
} BlockType;
class Compiler {
public:
  Compiler() = default;
  Compiler(const Compiler&) = delete;
  Compiler& operator=(const Compiler&) = delete;
  Compiler(Compiler&&) = delete;
  Compiler& operator=(Compiler&&) = delete;

  // The parser of the compiler which contains all the parsing context for the
  // current compilation.
  Parser parser;

  // Each module will be compiled with it's own compiler and a module is
  // imported, a new compiler is created for that module and it'll be added to
  // the linked list of compilers at the begining. PKVM will use this compiler
  // reference as a root object (objects which won't garbage collected) and
  // the chain of compilers will be marked at the marking phase.
  //
  // Here is how the chain change when a new compiler (compiler_3) created.
  //
  //     PKVM -> compiler_2 -> compiler_1 -> NULL
  //
  //     PKVM -> compiler_3 -> compiler_2 -> compiler_1 -> NULL
  //
  Compiler* next_compiler;

  const CompileOptions* options; //< To configure the compilation.

  Module* module;  //< Current module that's being compiled.
  Loop* loop;      //< Current loop the we're parsing.
  Func* func;      //< Current function we're parsing.

  // Current depth the compiler in (-1 means top level) 0 means function
  // level and > 0 is inner scope.
  int scope_depth;

  // True if the last statement is a new local variable assignment. Because
  // the assignment is different than regular assignment and use this boolean
  // to tell the compiler that dont pop it's assigned value because the value
  // itself is the local.
  bool new_local;

  // Will be true when parsing an "l-value" which can be assigned to a value
  // using the assignment operator ('='). ie. 'a = 42' here a is an "l-value"
  // and the 42 is a "r-value" so the assignment is consumed and compiled.
  // Consider '42 = a' where 42 is a "r-value" which cannot be assigned.
  // Similarly 'a = 1 + b = 2' the expression '(1 + b)' is a "r value" and
  // the assignment here is invalid, however 'a = 1 + (b = 2)' is valid because
  // the 'b' is an "l-value" and can be assigned but the '(b = 2)' is a
  // "r-value".
  bool l_value;

  // We can do a new assignment inside an expression however we shouldn't
  // define a new one, since in pocketlang both assignment and definition
  // are syntactically the same, we use [can_define] "context" to prevent
  // such assignments.
  bool can_define;

  // This value will be true after parsing a call expression, for every other
  // Expressions it'll be false. This is **ONLY** to be used when compiling a
  // return statement to check if the last parsed expression is a call to
  // perform a tail call optimization (anywhere else this below boolean is
  // meaningless).
  bool is_last_call;

  // Since the compiler manually call some builtin functions we need to cache
  // the index of the functions in order to prevent search for them each time.
  int bifn_list_join;

  // Compiler methods
  void init(PKVM* vm, const char* source, Module* module,
            const CompileOptions* options);
  void syntaxError(Token tk, const char* fmt, ...);
  void semanticError(Token tk, const char* fmt, ...);
  void resolveError(Token tk, const char* fmt, ...);
  void checkMaxConstantsReached(int index);
  void eatString(bool single_quote);
  void eatNumber();
  void lexToken();
  _TokenType peek();
  bool match(_TokenType expected);
  void consume(_TokenType expected, const char* err_msg);
  bool matchLine();
  void skipNewLines();
  bool matchEndStatement();
  void consumeEndStatement();
  void consumeStartBlock(_TokenType delimiter);
  bool matchAssignment();
  int addUpvalue(Func* func, int index, bool is_immediate);
  int findUpvalue(Func* func, std::string_view name);
  NameSearchResult searchName(std::string_view name);
  void emitStoreGlobal(int index);
  void emitPushValue(NameDefnType type, int index);
  void emitStoreValue(NameDefnType type, int index);
  void _compileCall(Opcode call_type, int method);
  bool _compileOptionalParanCall(int method);
  void exprLiteral();
  void exprInterpolation();
  void exprFunction();
  void exprName();
  void exprOr();
  void exprAnd();
  void exprBinaryOp();
  void exprUnaryOp();
  void exprGrouping();
  void exprList();
  void exprMap();
  void exprCall();
  void exprAttrib();
  void exprSubscript();
  void exprValue();
  void exprSelf();
  void exprSuper();
  void parsePrecedence(Precedence precedence);
  int addVariable(const char* name, uint32_t length, int line);
  void addForward(int instruction, Fn* fn, Token* tkname);
  int addConstant(Var value);
  void enterBlock();
  void changeStack(int num);
  int popLocals(int depth);
  void exitBlock();
  void pushFunc(Func* fn, Function* func, FuncType type);
  void popFunc();
  int emitByte(int byte);
  int emitShort(int arg);
  void emitOpcode(Opcode opcode);
  void emitLoopJump();
  void emitAssignedOp(_TokenType assignment);
  void emitFunctionEnd();
  void patchJump(int addr_index);
  void patchListSize(int size_index, int size);
  void patchForward(Fn* fn, int index, int name);
  void compileStatement();
  void compileBlockBody(BlockType type);
  int compileClass();
  bool matchOperatorMethod(const char** name, int* length, int* argc);
  void compileFunction(FuncType fn_type);
  Token compileImportPath();
  void compileFromImport();
  void compileRegularImport();
  void compileExpression();
  void compileIfStatement(bool elif);
  void compileWhileStatement();
  void compileForStatement();
  void compileTopLevelStatement();
};

typedef struct {
  int params;
  int stack;
} OpInfo;

static OpInfo opcode_info[] = {
  #define OPCODE(name, params, stack) { params, stack },
  #include "opcodes.h"  //<< AMALG_INLINE >>
  #undef OPCODE
};

/*****************************************************************************/
/* INITALIZATION FUNCTIONS                                                   */
/*****************************************************************************/

// FIXME:
// This forward declaration can be removed once the interpolated string's
// "list_join" function replaced with BUILD_STRING opcode. (The declaration
// needed at compiler initialization function to find the "list_join" function.
static int findBuiltinFunction(const PKVM* vm, std::string_view name);

// This should be called once the compiler initialized (to access it's fields).
void Parser::init(PKVM* vm, Compiler* compiler,
                  const char* source, const char* path) {

  this->vm = vm;

  this->source = source;
  this->file_path = path;
  this->token_start = this->source;
  this->current_char = this->source;
  this->current_line = 1;

  this->previous.type = TK_ERROR;
  this->current.type = TK_ERROR;
  this->next.type = TK_ERROR;

  this->next.start = NULL;
  this->next.length = 0;
  this->next.line = 1;
  this->next.value = VAR_UNDEFINED;

  this->si_depth = 0;
  this->si_name_end = NULL;
  this->si_name_quote = '\0';

  this->forwards_count = 0;

  this->repl_mode = compiler->options && compiler->options->replMode();
  this->optional_call_paran = false;
  this->parsing_class = false;
  this->has_errors = false;
  this->has_syntax_error = false;
  this->need_more_lines = false;
}

void Compiler::init(PKVM* vm, const char* source,
                    Module* module, const CompileOptions* options) {

  memset(this, 0, sizeof(Compiler));

  this->next_compiler = NULL;

  this->module = module;
  this->options = options;

  this->scope_depth = DEPTH_GLOBAL;

  this->loop = NULL;
  this->func = NULL;

  this->can_define = true;
  this->new_local = false;
  this->is_last_call = false;

  const char* source_path = "@??";
  if (module->path != NULL) {
    source_path = module->path->data;

  } else if (options && options->replMode()) {
    source_path = "@REPL";
  }

  this->parser.init(vm, this, source, source_path);

  // Cache the required built functions.
  this->bifn_list_join = findBuiltinFunction(vm, "list_join");
  ASSERT(this->bifn_list_join >= 0, OOPS);
}

/*****************************************************************************/
/* ERROR HANDLERS                                                            */
/*****************************************************************************/

// Internal error report function for lexing and parsing.
void Parser::reportError(Token tk, const char* fmt, va_list args) {

  this->has_errors = true;

  PKVM* vm = this->vm;
  if (vm->config.stderr_write == NULL) return;

  // If the source is incomplete we're not printing an error message,
  // instead return PK_RESULT_UNEXPECTED_EOF to the host.
  if (this->need_more_lines) {
    ASSERT(this->repl_mode, OOPS);
    return;
  }

  reportCompileTimeError(vm, this->file_path, tk.line, this->source,
                         tk.start, tk.length, fmt, args);

}

// Error caused when parsing. The associated token assumed to be last consumed
// which is [parser->previous].
void Compiler::syntaxError(Token tk, const char* fmt, ...) {
  Parser* parser = &this->parser;

  // Only one syntax error is reported.
  if (parser->has_syntax_error) return;

  parser->has_syntax_error = true;
  va_list args;
  va_start(args, fmt);
  parser->reportError(tk, fmt, args);
  va_end(args);
}

void Compiler::semanticError(Token tk, const char* fmt, ...) {
  Parser* parser = &this->parser;

  // If the parser has synax errors, semantic errors are not reported.
  if (parser->has_syntax_error) return;

  va_list args;
  va_start(args, fmt);
  parser->reportError(tk, fmt, args);
  va_end(args);
}

// Error caused when trying to resolve forward names (maybe more in the
// future), Which will be called once after compiling the module and thus we
// need to pass the line number the error originated from.
void Compiler::resolveError(Token tk, const char* fmt, ...) {
  Parser* parser = &this->parser;

  va_list args;
  va_start(args, fmt);
  parser->reportError(tk, fmt, args);
  va_end(args);
}

// Check if the given [index] is greater than or equal to the maximum constants
// that a module can contain and report an error.
void Compiler::checkMaxConstantsReached(int index) {
  ASSERT(index >= 0, OOPS);
  if (index >= MAX_CONSTANTS) {
    semanticError(this->parser.previous,
        "A module should contain at most %d unique constants.", MAX_CONSTANTS);
  }
}

/*****************************************************************************/
/* LEXING                                                                    */
/*****************************************************************************/

// Forward declaration of lexer methods.

void Compiler::eatString(bool single_quote) {
  Parser* parser = &this->parser;

  pkByteBuffer buff;
  pkBufferInit(&buff);

  char quote = (single_quote) ? '\'' : '"';

  // For interpolated string it'll be TK_STRING_INTERP.
  _TokenType tk_type = TK_STRING;

  while (true) {
    char c = parser->eatChar();

    if (c == quote) break;

    if (c == '\0') {
      syntaxError(parser->makeErrToken(), "Non terminated string.");
      return;

      // Null byte is required by TK_EOF.
      parser->current_char--;
      break;
    }

    if (c == '$') {
      if (parser->si_depth < MAX_STR_INTERP_DEPTH) {
        tk_type = TK_STRING_INTERP;

        char c2 = parser->peekChar();
        if (c2 == '{') { // Expression interpolation (ie. "${expr}").
          parser->eatChar();
          parser->si_depth++;
          parser->si_quote[parser->si_depth - 1] = quote;
          parser->si_open_brace[parser->si_depth - 1] = 0;

        } else { // Name Interpolation.
          if (!utilIsName(c2)) {
            syntaxError(parser->makeErrToken(),
                        "Expected '{' or identifier after '$'.");
            return;

          } else { // Name interpolation (ie. "Hello $name!").

            // The pointer [ptr] will points to the character at where the
            // interpolated string ends. (ie. the next character after name
            // ends).
            const char* ptr = parser->current_char;
            while (utilIsName(*(ptr)) || utilIsDigit(*(ptr))) {
              ptr++;
            }
            parser->si_name_end = ptr;
            parser->si_name_quote = quote;
          }
        }

      } else {
        semanticError(parser->makeErrToken(),
                     "Maximum interpolation level reached (can only "
                     "interpolate upto depth %d).", MAX_STR_INTERP_DEPTH);
      }
      break;
    }

    if (c == '\\') {
      switch (parser->eatChar()) {
        case '"':  pkBufferWrite(&buff, parser->vm, '"'); break;
        case '\'': pkBufferWrite(&buff, parser->vm, '\''); break;
        case '\\': pkBufferWrite(&buff, parser->vm, '\\'); break;
        case 'n':  pkBufferWrite(&buff, parser->vm, '\n'); break;
        case 'r':  pkBufferWrite(&buff, parser->vm, '\r'); break;
        case 't':  pkBufferWrite(&buff, parser->vm, '\t'); break;
        case '\n': break; // Just ignore the next line.

        // '$' In pocketlang string is used for interpolation.
        case '$':  pkBufferWrite(&buff, parser->vm, '$'); break;

        // Hex literal in string should match `\x[0-9a-zA-Z][0-9a-zA-Z]`
        case 'x': {
          uint8_t val = 0;

          c = parser->eatChar();
          if (!utilIsCharHex(c)) {
            semanticError(parser->makeErrToken(),
                          "Invalid hex escape.");
            break;
          }

          val = utilCharHexVal(c);

          c = parser->eatChar();
          if (!utilIsCharHex(c)) {
            semanticError(parser->makeErrToken(),
              "Invalid hex escape.");
            break;
          }

          val = (val << 4) | utilCharHexVal(c);

          pkBufferWrite(&buff, parser->vm, val);

        } break;

        case '\r':
          if (parser->matchChar('\n')) break;
          // Else fallthrough.

        default:

          semanticError(parser->makeErrToken(),
                        "Invalid escape character.");
          break;
      }
    } else {
      pkBufferWrite(&buff, parser->vm, c);
    }
  }

  // '\0' will be added by varNewSring();
  Var string = VAR_OBJ(newStringLength(parser->vm,
                       {(const char*)buff.data, buff.count}));

  pkBufferClear(&buff, parser->vm);

  parser->setNextValueToken(tk_type, string);
}

// Returns the current char of the compiler on.
char Parser::peekChar() {
  return *this->current_char;
}

// Returns the next char of the compiler on.
char Parser::peekNextChar() {
  if (peekChar() == '\0') return '\0';
  return *(this->current_char + 1);
}

// Advance the compiler by 1 char.
char Parser::eatChar() {
  char c = peekChar();
  this->current_char++;
  if (c == '\n') this->current_line++;
  return c;
}

// Complete lexing an identifier name.
void Parser::eatName() {

  char c = peekChar();
  while (utilIsName(c) || utilIsDigit(c)) {
    eatChar();
    c = peekChar();
  }

  const char* name_start = this->token_start;

  _TokenType type = TK_NAME;

  int length = (int)(this->current_char - name_start);
  for (int i = 0; _keywords[i].identifier != NULL; i++) {
    if (_keywords[i].length == length &&
      strncmp(name_start, _keywords[i].identifier, length) == 0) {
      type = _keywords[i].tk_type;
      break;
    }
  }

  setNextToken(type);
}

// Complete lexing a number literal.
void Compiler::eatNumber() {
  Parser* parser = &this->parser;

#define IS_BIN_CHAR(c) (((c) == '0') || ((c) == '1'))

  Var value = VAR_NULL; // The number value.
  char c = *parser->token_start;

  // Binary literal.
  if (c == '0' &&
      ((parser->peekChar() == 'b') || (parser->peekChar() == 'B'))) {
    parser->eatChar(); // Consume '0b'

    uint64_t bin = 0;
    c = parser->peekChar();
    if (!IS_BIN_CHAR(c)) {
      syntaxError(parser->makeErrToken(), "Invalid binary literal.");
      return;

    } else {
      do {
        // Consume the next digit.
        c = parser->peekChar();
        if (!IS_BIN_CHAR(c)) break;
        parser->eatChar();

        // Check the length of the binary literal.
        int length = (int)(parser->current_char - parser->token_start);
        if (length > STR_BIN_BUFF_SIZE - 2) { // -2: '-\0' 0b is in both side.
          semanticError(parser->makeErrToken(),
                        "Binary literal is too long.");
          break;
        }

        // "Append" the next digit at the end.
        bin = (bin << 1) | (c - '0');

      } while (true);
    }
    value = VAR_NUM((double)bin);

  } else if (c == '0' &&
             ((parser->peekChar() == 'x') || (parser->peekChar() == 'X'))) {
    parser->eatChar(); // Consume '0x'

    uint64_t hex = 0;
    c = parser->peekChar();

    // The first digit should be hex digit.
    if (!utilIsCharHex(c)) {
      syntaxError(parser->makeErrToken(), "Invalid hex literal.");
      return;

    } else {
      do {
        // Consume the next digit.
        c = parser->peekChar();
        if (!utilIsCharHex(c)) break;
        parser->eatChar();

        // Check the length of the binary literal.
        int length = (int)(parser->current_char - parser->token_start);
        if (length > STR_HEX_BUFF_SIZE - 2) { // -2: '-\0' 0x is in both side.
          semanticError(parser->makeErrToken(),
                        "Hex literal is too long.");
          break;
        }

        // "Append" the next digit at the end.
        hex = (hex << 4) | utilCharHexVal(c);

      } while (true);

      value = VAR_NUM((double)hex);
    }

  } else { // Regular number literal.

    while (utilIsDigit(parser->peekChar())) {
      parser->eatChar();
    }

    if (c != '.') { // Number starts with a decimal point.
      if (parser->peekChar() == '.' && utilIsDigit(parser->peekNextChar())) {
        parser->matchChar('.');
        while (utilIsDigit(parser->peekChar())) {
          parser->eatChar();
        }
      }
    }

    // Parse if in scientific notation format (MeN == M * 10 ** N).
    if (parser->matchChar('e') || parser->matchChar('E')) {

      if (parser->peekChar() == '+' || parser->peekChar() == '-') {
        parser->eatChar();
      }

      if (!utilIsDigit(parser->peekChar())) {
        syntaxError(parser->makeErrToken(), "Invalid number literal.");
        return;

      } else { // Eat the exponent.
        while (utilIsDigit(parser->peekChar())) parser->eatChar();
      }
    }

    errno = 0;
    value = VAR_NUM(atof(parser->token_start));
    if (errno == ERANGE) {
      const char* start = parser->token_start;
      int len = (int)(parser->current_char - start);
      semanticError(parser->makeErrToken(),
                    "Number literal is too large (%.*s).", len, start);
      value = VAR_NUM(0);
    }
  }

  parser->setNextValueToken(TK_NUMBER, value);
#undef IS_BIN_CHAR
}

// Read and ignore chars till it reach new line or EOF.
void Parser::skipLineComment() {
  char c;
  while ((c = peekChar()) != '\0') {
    // Don't eat new line it's not part of the comment.
    if (c == '\n') return;
    eatChar();
  }
}

// If the current char is [c] consume it and advance char by 1 and returns
// true otherwise returns false.
bool Parser::matchChar(char c) {
  if (peekChar() != c) return false;
  eatChar();
  return true;
}

// If the current char is [c] eat the char and add token two otherwise eat
// append token one.
void Parser::setNextTwoCharToken(char c, _TokenType one, _TokenType two) {
  if (matchChar(c)) {
    setNextToken(two);
  } else {
    setNextToken(one);
  }
}

// Returns an error token from the current position for reporting error.
Token Parser::makeErrToken() {
  Token tk;
  tk.type = TK_ERROR;
  tk.start = this->token_start;
  tk.length = (int)(this->current_char - this->token_start);
  tk.line = this->current_line;
  return tk;
}

// Initialize the next token as the type.
void Parser::setNextToken(_TokenType type) {
  Token* next = &this->next;
  next->type = type;
  next->start = this->token_start;
  next->length = (int)(this->current_char - this->token_start);
  next->line = this->current_line - ((type == TK_LINE) ? 1 : 0);
}

// Initialize the next token as the type and assign the value.
void Parser::setNextValueToken(_TokenType type, Var value) {
  setNextToken(type);
  this->next.value = value;
}

// Lex the next token and set it as the next token.
void Compiler::lexToken() {
  Parser* parser = &this->parser;

  parser->previous = parser->current;
  parser->current = parser->next;

  if (parser->current.type == TK_EOF) return;

  while (parser->peekChar() != '\0') {
    parser->token_start = parser->current_char;

    // If we're parsing a name interpolation and the current character is where
    // the name end, continue parsing the string.
    //
    //        "Hello $name!"
    //                    ^-- si_name_end
    //
    if (parser->si_name_end != NULL) {
      if (parser->current_char == parser->si_name_end) {
        parser->si_name_end = NULL;
        eatString(parser->si_name_quote == '\'');
        return;
      } else {
        ASSERT(parser->current_char < parser->si_name_end, OOPS);
      }
    }

    char c = parser->eatChar();
    switch (c) {

      case '{': {

        // If we're inside an interpolation, increase the open brace count
        // of the current depth.
        if (parser->si_depth > 0) {
          parser->si_open_brace[parser->si_depth - 1]++;
        }
        parser->setNextToken(TK_LBRACE);
        return;
      }

      case '}': {
        // If we're inside of an interpolated string.
        if (parser->si_depth > 0) {

          // No open braces, then end the expression and complete the string.
          if (parser->si_open_brace[parser->si_depth - 1] == 0) {

            char quote = parser->si_quote[parser->si_depth - 1];
            parser->si_depth--; //< Exit the depth.
            eatString(quote == '\'');
            return;

          } else { // Decrease the open brace at the current depth.
            parser->si_open_brace[parser->si_depth - 1]--;
          }
        }

        parser->setNextToken(TK_RBRACE);
        return;
      }

      case ',': parser->setNextToken(TK_COMMA); return;
      case ':': parser->setNextToken(TK_COLLON); return;
      case ';': parser->setNextToken(TK_SEMICOLLON); return;
      case '#': parser->skipLineComment(); break;
      case '(': parser->setNextToken(TK_LPARAN); return;
      case ')': parser->setNextToken(TK_RPARAN); return;
      case '[': parser->setNextToken(TK_LBRACKET); return;
      case ']': parser->setNextToken(TK_RBRACKET); return;
      case '%':
        parser->setNextTwoCharToken('=', TK_PERCENT, TK_MODEQ);
        return;

      case '~': parser->setNextToken(TK_TILD); return;

      case '&':
        parser->setNextTwoCharToken('=', TK_AMP, TK_ANDEQ);
        return;

      case '|':
        parser->setNextTwoCharToken('=', TK_PIPE, TK_OREQ);
        return;

      case '^':
        parser->setNextTwoCharToken('=', TK_CARET, TK_XOREQ);
        return;

      case '\n': parser->setNextToken(TK_LINE); return;

      case ' ':
      case '\t':
      case '\r': {
        c = parser->peekChar();
        while (c == ' ' || c == '\t' || c == '\r') {
          parser->eatChar();
          c = parser->peekChar();
        }
        break;
      }

      case '.':
        if (parser->matchChar('.')) {
          parser->setNextToken(TK_DOTDOT); // '..'
        } else if (utilIsDigit(parser->peekChar())) {
          parser->eatChar();   // Consume the decimal point.
          eatNumber(); // Consume the rest of the number
          if (parser->has_syntax_error) return;
        } else {
          parser->setNextToken(TK_DOT);    // '.'
        }
        return;

      case '=':
        parser->setNextTwoCharToken('=', TK_EQ, TK_EQEQ);
        return;

      case '!':
        parser->setNextTwoCharToken('=', TK_NOT, TK_NOTEQ);
        return;

      case '>':
        if (parser->matchChar('>')) {
          parser->setNextTwoCharToken('=', TK_SRIGHT, TK_SRIGHTEQ);
        } else {
          parser->setNextTwoCharToken('=', TK_GT, TK_GTEQ);
        }
        return;

      case '<':
        if (parser->matchChar('<')) {
          parser->setNextTwoCharToken('=', TK_SLEFT, TK_SLEFTEQ);
        } else {
          parser->setNextTwoCharToken('=', TK_LT, TK_LTEQ);
        }
        return;

      case '+':
        parser->setNextTwoCharToken('=', TK_PLUS, TK_PLUSEQ);
        return;

      case '-':
        if (parser->matchChar('=')) {
          parser->setNextToken(TK_MINUSEQ);  // '-='
        } else if (parser->matchChar('>')) {
          parser->setNextToken(TK_ARROW);    // '->'
        } else {
          parser->setNextToken(TK_MINUS);    // '-'
        }
        return;

      case '*':
        if (parser->matchChar('*')) {
          parser->setNextTwoCharToken('=', TK_STARSTAR, TK_POWEQ);
        } else {
          parser->setNextTwoCharToken('=', TK_STAR, TK_STAREQ);
        }
        return;

      case '/':
        parser->setNextTwoCharToken('=', TK_FSLASH, TK_DIVEQ);
        return;

      case '"': eatString(false); return;

      case '\'': eatString(true); return;

      default: {

        if (utilIsDigit(c)) {
          eatNumber();
          if (parser->has_syntax_error) return;

        } else if (utilIsName(c)) {
          parser->eatName();

        } else {
          parser->setNextToken(TK_ERROR);

          if (c >= 32 && c <= 126) {
            syntaxError(parser->next,
                        "Invalid character '%c'", c);
          } else {
            syntaxError(parser->next,
                        "Invalid byte 0x%x", (uint8_t)c);
          }
        }
        return;
      }
    }
  }

  parser->token_start = parser->current_char;
  parser->setNextToken(TK_EOF);
}

/*****************************************************************************/
/* PARSING                                                                   */
/*****************************************************************************/

// Returns current token type without lexing a new token.
_TokenType Compiler::peek() {
  return this->parser.current.type;
}

// Consume the current token if it's expected and lex for the next token
// and return true otherwise return false.
bool Compiler::match(_TokenType expected) {
  if (peek() != expected) return false;

  lexToken();
  if (this->parser.has_syntax_error) return false;

  return true;
}

// Consume the the current token and if it's not [expected] emits error log
// and continue parsing for more error logs.
void Compiler::consume(_TokenType expected, const char* err_msg) {

  lexToken();
  if (this->parser.has_syntax_error) return;

  Token *prev = &this->parser.previous;
  if (prev->type != expected) {
    syntaxError(*prev, "%s", err_msg);
    return;
  }
}

// Match one or more lines and return true if there any.
bool Compiler::matchLine() {

  bool consumed = false;

  if (peek() == TK_LINE) {
    while (peek() == TK_LINE) {
      lexToken();
      if (this->parser.has_syntax_error) return false;
    }
    consumed = true;
  }

  // If we're running on REPL mode, at the EOF and compile time error occurred,
  // signal the host to get more lines and try re-compiling it.
  if (this->parser.repl_mode && !this->parser.has_errors) {
    if (peek() == TK_EOF) {
      this->parser.need_more_lines = true;
    }
  }

  return consumed;
}

// Will skip multiple new lines.
void Compiler::skipNewLines() {
  matchLine();
}

// Match semi collon, multiple new lines or peek 'end', 'else', 'elif'
// keywords.
bool Compiler::matchEndStatement() {
  if (match(TK_SEMICOLLON)) {
    skipNewLines();
    return true;
  }
  if (matchLine() || peek() == TK_EOF)
    return true;

  // In the below statement we don't require any new lines or semicolons.
  // 'if cond then stmnt1 else if cond2 then stmnt2 else stmnt3 end'
  if (peek() == TK_END
    || peek() == TK_ELSE
    || peek() == TK_ELIF)
    return true;

  return false;
}

// Consume semi collon, multiple new lines or peek 'end' keyword.
void Compiler::consumeEndStatement() {
  if (!matchEndStatement()) {
    syntaxError(this->parser.current,
                "Expected statement end with '\\n' or ';'.");
    return;
  }
}

// Match optional "do" or "then" keyword and new lines.
void Compiler::consumeStartBlock(_TokenType delimiter) {
  bool consumed = false;

  // Match optional "do" or "then".
  if (delimiter == TK_DO || delimiter == TK_THEN) {
    if (match(delimiter))
      consumed = true;
  }

  if (matchLine())
    consumed = true;

  if (!consumed) {
    const char* msg;
    if (delimiter == TK_DO) msg = "Expected enter block with newline or 'do'.";
    else msg = "Expected enter block with newline or 'then'.";
    syntaxError(this->parser.previous, msg);
    return;
  }
}

// Returns a optional compound assignment.
bool Compiler::matchAssignment() {
  if (match(TK_EQ))       return true;
  if (match(TK_PLUSEQ))   return true;
  if (match(TK_MINUSEQ))  return true;
  if (match(TK_STAREQ))   return true;
  if (match(TK_DIVEQ))    return true;
  if (match(TK_MODEQ))    return true;
  if (match(TK_POWEQ))    return true;
  if (match(TK_ANDEQ))    return true;
  if (match(TK_OREQ))     return true;
  if (match(TK_XOREQ))    return true;
  if (match(TK_SRIGHTEQ)) return true;
  if (match(TK_SLEFTEQ))  return true;

  return false;
}

/*****************************************************************************/
/* NAME SEARCH (AT COMPILATION PHASE)                                        */
/*****************************************************************************/

// Find the builtin function name and returns it's index in the builtins array
// if not found returns -1.
static int findBuiltinFunction(const PKVM* vm, std::string_view name) {
  for (int i = 0; i < vm->builtins_count; i++) {
    if (vm->builtins_funcs[i]->fn->name == name) {
      return i;
    }
  }
  return -1;
}

// Find the builtin classes name and returns it's index in the VM's builtin
// classes array, if not found returns -1.
static int findBuiltinClass(const PKVM* vm, std::string_view name) {
  for (int i = 0; i < PK_INSTANCE; i++) {
    const String* cls_name = vm->builtin_classes[i]->name;
    if (cls_name->length == name.size() &&
        strncmp(cls_name->data, name.data(), name.size()) == 0) {
      return i;
    }
  }
  return -1;
}

// Find the local with the [name] in the given function [func] and return
// it's index, if not found returns -1.
static int findLocal(Func* func, std::string_view name) {
  ASSERT(func != NULL, OOPS);
  for (int i = 0; i < func->local_count; i++) {
    if ((size_t)func->locals[i].length != name.size()) continue;
    if (strncmp(func->locals[i].name, name.data(), name.size()) == 0) {
      return i;
    }
  }
  return -1;
}

// Add the upvalue to the given function and return it's index, if the upvalue
// already present in the function's upvalue array it'll return it.
int Compiler::addUpvalue(Func* func, int index, bool is_immediate) {

  // Search the upvalue in the existsing upvalues array.
  for (int i = 0; i < func->ptr->upvalue_count; i++) {
    UpvalueInfo info = func->upvalues[i];
    if (info.index == index && info.is_immediate == is_immediate) {
      return i;
    }
  }

  if (func->ptr->upvalue_count == MAX_UPVALUES) {
    semanticError(this->parser.previous,
            "A function cannot capture more thatn %d upvalues.", MAX_UPVALUES);
    return -1;
  }

  func->upvalues[func->ptr->upvalue_count].index = index;
  func->upvalues[func->ptr->upvalue_count].is_immediate = is_immediate;
  return func->ptr->upvalue_count++;
}

// Search for an upvalue with the given [name] for the current function [func].
// If an upvalue found, it'll add the upvalue info to the upvalue infor array
// of the [func] and return the index of the upvalue in the current function's
// upvalues array.
int Compiler::findUpvalue(Func* func, std::string_view name) {
  // TODO:
  // check if the function is a method of a class and return -1 for them as
  // well (once methods implemented).
  //
  // Toplevel functions cannot have upvalues.
  if (func->depth <= DEPTH_GLOBAL) return -1;

  // Search in the immediate enclosing function's locals.
  int index = findLocal(func->outer_func, name);
  if (index != -1) {

    // Mark the locals as an upvalue to close it when it goes out of the scope.
    func->outer_func->locals[index].is_upvalue = true;

    // Add upvalue to the function and return it's index.
    return addUpvalue(func, index, true);
  }

  // Recursively search for the upvalue in the outer function. If we found one
  // all the outer function in the chain would have captured the upvalue for
  // the local, we can add it to the current function as non-immediate upvalue.
  index = findUpvalue(func->outer_func, name);

  if (index != -1) {
    return addUpvalue(func, index, false);
  }

  // If we reached here, the upvalue doesn't exists.
  return -1;
}

// Will check if the name already defined.
NameSearchResult Compiler::searchName(std::string_view name) {

  NameSearchResult result;
  result.type = NAME_NOT_DEFINED;

  int index; // For storing the search result below.

  // Search through locals.
  index = findLocal(this->func, name);
  if (index != -1) {
    result.type = NAME_LOCAL_VAR;
    result.index = index;
    return result;
  }

  // Search through upvalues.
  index = findUpvalue(this->func, name);
  if (index != -1) {
    result.type = NAME_UPVALUE;
    result.index = index;
    return result;
  }

  // Search through globals.
  index = moduleGetGlobalIndex(this->module, name);
  if (index != -1) {
    result.type = NAME_GLOBAL_VAR;
    result.index = index;
    return result;
  }

  // Search through builtin functions.
  index = findBuiltinFunction(this->parser.vm, name);
  if (index != -1) {
    result.type = NAME_BUILTIN_FN;
    result.index = index;
    return result;
  }

  index = findBuiltinClass(this->parser.vm, name);
  if (index != -1) {
    result.type = NAME_BUILTIN_TY;
    result.index = index;
    return result;
  }

  return result;
}

/*****************************************************************************/
/* PARSING GRAMMAR                                                           */
/*****************************************************************************/

// Forward declaration of codegen functions.

// Forward declaration of grammar functions.

// true, false, null, self.

#define NO_RULE { NULL,  NULL,  PREC_NONE }
#define NO_INFIX PREC_NONE
#define _C(fn) &Compiler::fn

GrammarRule rules[] = { // Prefix        Infix           Infix Precedence
  /* TK_ERROR      */  NO_RULE,
  /* TK_EOF        */  NO_RULE,
  /* TK_LINE       */  NO_RULE,
  /* TK_DOT        */ { NULL,            _C(exprAttrib),  PREC_ATTRIB },
  /* TK_DOTDOT     */ { NULL,            _C(exprBinaryOp),PREC_RANGE },
  /* TK_COMMA      */  NO_RULE,
  /* TK_COLLON     */  NO_RULE,
  /* TK_SEMICOLLON */  NO_RULE,
  /* TK_HASH       */  NO_RULE,
  /* TK_LPARAN     */ { _C(exprGrouping),_C(exprCall),    PREC_CALL },
  /* TK_RPARAN     */  NO_RULE,
  /* TK_LBRACKET   */ { _C(exprList),    _C(exprSubscript),PREC_SUBSCRIPT },
  /* TK_RBRACKET   */  NO_RULE,
  /* TK_LBRACE     */ { _C(exprMap),     NULL,            NO_INFIX },
  /* TK_RBRACE     */  NO_RULE,
  /* TK_PERCENT    */ { NULL,            _C(exprBinaryOp),PREC_FACTOR },
  /* TK_TILD       */ { _C(exprUnaryOp), NULL,            NO_INFIX },
  /* TK_AMP        */ { NULL,            _C(exprBinaryOp),PREC_BITWISE_AND },
  /* TK_PIPE       */ { NULL,            _C(exprBinaryOp),PREC_BITWISE_OR },
  /* TK_CARET      */ { NULL,            _C(exprBinaryOp),PREC_BITWISE_XOR },
  /* TK_ARROW      */  NO_RULE,
  /* TK_PLUS       */ { _C(exprUnaryOp), _C(exprBinaryOp),PREC_TERM },
  /* TK_MINUS      */ { _C(exprUnaryOp), _C(exprBinaryOp),PREC_TERM },
  /* TK_STAR       */ { NULL,            _C(exprBinaryOp),PREC_FACTOR },
  /* TK_FSLASH     */ { NULL,            _C(exprBinaryOp),PREC_FACTOR },
  /* TK_STARSTAR   */ { NULL,            _C(exprBinaryOp),PREC_EXPONENT },
  /* TK_BSLASH     */  NO_RULE,
  /* TK_EQ         */  NO_RULE,
  /* TK_GT         */ { NULL,            _C(exprBinaryOp),PREC_COMPARISION },
  /* TK_LT         */ { NULL,            _C(exprBinaryOp),PREC_COMPARISION },
  /* TK_EQEQ       */ { NULL,            _C(exprBinaryOp),PREC_EQUALITY },
  /* TK_NOTEQ      */ { NULL,            _C(exprBinaryOp),PREC_EQUALITY },
  /* TK_GTEQ       */ { NULL,            _C(exprBinaryOp),PREC_COMPARISION },
  /* TK_LTEQ       */ { NULL,            _C(exprBinaryOp),PREC_COMPARISION },
  /* TK_PLUSEQ     */  NO_RULE,
  /* TK_MINUSEQ    */  NO_RULE,
  /* TK_STAREQ     */  NO_RULE,
  /* TK_DIVEQ      */  NO_RULE,
  /* TK_MODEQ      */  NO_RULE,
  /* TK_POWEQ      */  NO_RULE,
  /* TK_ANDEQ      */  NO_RULE,
  /* TK_OREQ       */  NO_RULE,
  /* TK_XOREQ      */  NO_RULE,
  /* TK_SRIGHT     */ { NULL,            _C(exprBinaryOp),PREC_BITWISE_SHIFT },
  /* TK_SLEFT      */ { NULL,            _C(exprBinaryOp),PREC_BITWISE_SHIFT },
  /* TK_SRIGHTEQ   */  NO_RULE,
  /* TK_SLEFTEQ    */  NO_RULE,
  /* TK_CLASS      */  NO_RULE,
  /* TK_FROM       */  NO_RULE,
  /* TK_IMPORT     */  NO_RULE,
  /* TK_AS         */  NO_RULE,
  /* TK_DEF        */  NO_RULE,
  /* TK_EXTERN     */  NO_RULE,
  /* TK_FN         */ { _C(exprFunction),NULL,            NO_INFIX },
  /* TK_END        */  NO_RULE,
  /* TK_NULL       */ { _C(exprValue),   NULL,            NO_INFIX },
  /* TK_IN         */ { NULL,            _C(exprBinaryOp),PREC_TEST },
  /* TK_IS         */ { NULL,            _C(exprBinaryOp),PREC_TEST },
  /* TK_AND        */ { NULL,            _C(exprAnd),     PREC_LOGICAL_AND },
  /* TK_OR         */ { NULL,            _C(exprOr),      PREC_LOGICAL_OR },
  /* TK_NOT        */ { _C(exprUnaryOp), NULL,            PREC_UNARY },
  /* TK_TRUE       */ { _C(exprValue),   NULL,            NO_INFIX },
  /* TK_FALSE      */ { _C(exprValue),   NULL,            NO_INFIX },
  /* TK_SELF       */ { _C(exprSelf),    NULL,            NO_INFIX },
  /* TK_SUPER      */ { _C(exprSuper),   NULL,            NO_INFIX },
  /* TK_DO         */  NO_RULE,
  /* TK_THEN       */  NO_RULE,
  /* TK_WHILE      */  NO_RULE,
  /* TK_FOR        */  NO_RULE,
  /* TK_IF         */  NO_RULE,
  /* TK_ELIF       */  NO_RULE,
  /* TK_ELSE       */  NO_RULE,
  /* TK_BREAK      */  NO_RULE,
  /* TK_CONTINUE   */  NO_RULE,
  /* TK_RETURN     */  NO_RULE,
  /* TK_NAME       */ { _C(exprName),    NULL,            NO_INFIX },
  /* TK_NUMBER     */ { _C(exprLiteral), NULL,            NO_INFIX },
  /* TK_STRING     */ { _C(exprLiteral), NULL,            NO_INFIX },
  /* TK_STRING_INTERP */ { _C(exprInterpolation), NULL,   NO_INFIX },
};

#undef _C

static GrammarRule* getRule(_TokenType type) {
  return &(rules[(int)type]);
}

// FIXME:
// This function is used by the import system, remove this function (and move
// it to emitStoreName()) after import system refactored.
//
// Store the value at the stack top to the global at the [index].
void Compiler::emitStoreGlobal(int index) {
  emitOpcode(OP_STORE_GLOBAL);
  emitByte(index);
}

// Emit opcode to push the value of [type] at the [index] in it's array.
void Compiler::emitPushValue(NameDefnType type, int index) {
  ASSERT(index >= 0, OOPS);

  switch (type) {
    case NAME_NOT_DEFINED: {
      if (this->parser.has_errors) {
        return;
      }
      UNREACHABLE();
    }

    case NAME_LOCAL_VAR:
      if (index < 9) { //< 0..8 locals have single opcode.
        emitOpcode((Opcode)(OP_PUSH_LOCAL_0 + index));
      } else {
        emitOpcode(OP_PUSH_LOCAL_N);
        emitByte(index);
      }
      return;

    case NAME_UPVALUE:
      emitOpcode(OP_PUSH_UPVALUE);
      emitByte(index);
      return;

    case NAME_GLOBAL_VAR:
      emitOpcode(OP_PUSH_GLOBAL);
      emitByte(index);
      return;

    case NAME_BUILTIN_FN:
      emitOpcode(OP_PUSH_BUILTIN_FN);
      emitByte(index);
      return;

    case NAME_BUILTIN_TY:
      emitOpcode(OP_PUSH_BUILTIN_TY);
      emitByte(index);
      return;
  }
}

// Emit opcode to store the stack top value to the named value to the [type]
// at the [index] in it's array.
void Compiler::emitStoreValue(NameDefnType type, int index) {
  ASSERT(index >= 0, OOPS);

  switch (type) {
    case NAME_NOT_DEFINED:
    case NAME_BUILTIN_FN:
    case NAME_BUILTIN_TY: {
      if (this->parser.has_errors) return;
      UNREACHABLE();
    }

    case NAME_LOCAL_VAR:
      if (index < 9) { //< 0..8 locals have single opcode.
        emitOpcode((Opcode)(OP_STORE_LOCAL_0 + index));
      } else {
        emitOpcode(OP_STORE_LOCAL_N);
        emitByte(index);
      }
      return;

    case NAME_UPVALUE:
      emitOpcode(OP_STORE_UPVALUE);
      emitByte(index);
      return;

    case NAME_GLOBAL_VAR:
      emitStoreGlobal(index);
      return;
  }
}

// This function is reused between calls and method calls. if the [call_type]
// is OP_METHOD_CALL the [method] should refer a string in the module's
// constant pool, otherwise it's ignored.
void Compiler::_compileCall(Opcode call_type, int method) {
  ASSERT((call_type == OP_CALL) ||
    (call_type == OP_METHOD_CALL) ||
    (call_type == OP_SUPER_CALL), OOPS);
  // Compile parameters.
  int argc = 0;

  if (this->parser.optional_call_paran) {
    this->parser.optional_call_paran = false;
    compileExpression();
    argc = 1;

  } else {
    if (!match(TK_RPARAN)) {
      do {
        skipNewLines();
        compileExpression();
        skipNewLines();
        argc++;
      } while (match(TK_COMMA));
      consume(TK_RPARAN, "Expected ')' after parameter list.");
    }
  }

  emitOpcode(call_type);

  emitByte(argc);

  if ((call_type == OP_METHOD_CALL) || (call_type == OP_SUPER_CALL)) {
    ASSERT_INDEX(method, (int)this->module->constants.count);
    emitShort(method);
  }

  // After the call the arguments will be popped and the callable
  // will be replaced with the return value.
  changeStack(-argc);
}

// Like lua, we're omitting the paranthese for literals, it'll check for
// literals that can be passed for no pranthese call (a syntax sugar) and
// emit the call. Return true if such call matched. If [method] >= 0 it'll
// compile a method call otherwise a regular call.
bool Compiler::_compileOptionalParanCall(int method) {
  static _TokenType tk[] = {
    TK_FN,
    //TK_STRING,
    //TK_STRING_INTERP,
    TK_ERROR, // Sentinel to stop the iteration.
  };

  for (int i = 0; tk[i] != TK_ERROR; i++) {
    if (peek() == tk[i]) {
      this->parser.optional_call_paran = true;
      Opcode call_type = ((method >= 0) ? OP_METHOD_CALL : OP_CALL);
      _compileCall(call_type, method);
      return true;
    }
  }

  return false;
}

void Compiler::exprLiteral() {
  Token* value = &this->parser.previous;
  int index = addConstant(value->value);
  emitOpcode(OP_PUSH_CONSTANT);
  emitShort(index);
}

// Consider the bellow string.
//
//     "Hello $name!"
//
// This will be compiled as:
//
//     list_join(["Hello ", name, "!"])
//
void Compiler::exprInterpolation() {
  emitOpcode(OP_PUSH_BUILTIN_FN);
  emitByte(this->bifn_list_join);

  emitOpcode(OP_PUSH_LIST);
  int size_index = emitShort(0);

  int size = 0;
  do {
    // Push the string on the stack and append it to the list.
    exprLiteral();
    emitOpcode(OP_LIST_APPEND);
    size++;

    // Compile the expression and append it to the list.
    skipNewLines();
    compileExpression();
    emitOpcode(OP_LIST_APPEND);
    size++;
    skipNewLines();
  } while (match(TK_STRING_INTERP));

  // The last string is not TK_STRING_INTERP but it would be
  // TK_STRING. Apped it.
  // Optimize case last string could be empty. Skip it.
  consume(TK_STRING, "Non terminated interpolated string.");
  if (this->parser.previous.type == TK_STRING /* != if syntax error. */) {
    ASSERT(IS_OBJ_TYPE(this->parser.previous.value, OBJ_STRING), OOPS);
    String* str = (String*)AS_OBJ(this->parser.previous.value);
    if (str->length != 0) {
      exprLiteral();
      emitOpcode(OP_LIST_APPEND);
      size++;
    }
  }

  patchListSize(size_index, size);

  // Call the list_join function (which is at the stack top).
  emitOpcode(OP_CALL);
  emitByte(1);

  // After the above call, the lits and the "list_join" function will be popped
  // from the stack and a string will be pushed. The so the result stack effect
  // is -1.
  changeStack(-1);

}

void Compiler::exprFunction() {
  bool can_define = this->can_define;

  this->can_define = true;
  compileFunction(FUNC_LITERAL);
  this->can_define = can_define;
}

void Compiler::exprName() {

  Token tkname = this->parser.previous;

  const char* start = tkname.start;
  int length = tkname.length;
  int line = tkname.line;
  NameSearchResult result = searchName({start, (size_t)length});

  if (this->l_value && matchAssignment()) {
    _TokenType assignment = this->parser.previous.type;
    skipNewLines();

    // Type of the name that's being assigned. Could only be local, global
    // or an upvalue.
    NameDefnType name_type = result.type;
    int index = result.index; // Index of the name in it's array.

    // Will be set to true if the name is a new local.
    bool new_local = false;

    if (assignment == TK_EQ) { // name = (expr);

      // Assignment to builtin functions will override the name and it'll
      // become a local or global variable. Note that if the names is a global
      // which hasent defined yet we treat that as a local (no global keyword
      // like python does) and it's recommented to define all the globals
      // before entering a local scope.

      if (result.type == NAME_NOT_DEFINED ||
          result.type == NAME_BUILTIN_FN  ||
          result.type == NAME_BUILTIN_TY ) {
        name_type = (this->scope_depth == DEPTH_GLOBAL)
                    ? NAME_GLOBAL_VAR
                    : NAME_LOCAL_VAR;
        index = addVariable(start, length, line);

        // We cannot set `this->new_local = true;` here since there is an
        // expression after the assignment pending. We'll update it once the
        // expression is compiled.
        if (name_type == NAME_LOCAL_VAR) {
          new_local = true;
        }

        if (!this->can_define) {
          semanticError(tkname,
            "Variable definition isn't allowed here.");
        }
      }

      // Compile the assigned value.
      bool can_define = this->can_define;
      this->can_define = false;
      compileExpression();
      this->can_define = can_define;

    } else { // name += / -= / *= ... = (expr);

      if (result.type == NAME_NOT_DEFINED) {

        // TODO:
        // Add to forward names. Create result.type as NAME_FORWARD
        // and use emitPushName, emitStoreName for here and bellow.
        semanticError(tkname,
                      "Name '%.*s' is not defined.", length, start);
      }

      // Push the named value.
      emitPushValue(name_type, index);

      // Compile the RHS of the assigned operation.
      compileExpression();

      // Do the arithmatic operation of the assignment.
      emitAssignedOp(assignment);
    }

    // If it's a new local we don't have to store it, it's already at it's
    // stack slot.
    if (new_local) {
      // This will prevent the assignment from being popped out from the
      // stack since the assigned value itself is the local and not a temp.
      this->new_local = true;

      // Ensure the local variable's index is equals to the stack top index.
      // If the compiler has errors, we cannot and don't have to assert.
      ASSERT(this->parser.has_errors ||
             (this->func->stack_size - 1) == index, OOPS);

    } else {
      // The assigned value or the result of the operator will be at the top of
      // the stack by now. Store it.
      emitStoreValue(name_type, index);
    }

  } else { // Just the name and no assignment followed by.

    // The name could be a global value which hasn't been defined at this
    // point. We add an implicit forward declaration and once this expression
    // executed the value could be initialized only if the expression is at
    // a local depth.
    if (result.type == NAME_NOT_DEFINED) {
      if (this->scope_depth == DEPTH_GLOBAL) {
        semanticError(tkname,
                      "Name '%.*s' is not defined.", length, start);
      } else {
        emitOpcode(OP_PUSH_GLOBAL);
        int index = emitByte(0xff);
        addForward(index, _FN, &tkname);
      }
    } else {
      emitPushValue(result.type, result.index);
    }

    _compileOptionalParanCall(-1);
  }

}

// Compiling (expr a) or (expr b)
//
//            (expr a)
//             |  At this point (expr a) is at the stack top.
//             V
//        .-- (OP_OR [offset])
//        |    |  if true short circuit and skip (expr b)
//        |    |  otherwise pop (expr a) and continue.
//        |    V
//        |   (expr b)
//        |    |  At this point (expr b) is at the stack top.
//        |    V
//        '->  (...)
//              At this point stack top would be
//              either (expr a) or (expr b)
//
// Compiling 'and' expression is also similler but we jump if the (expr a) is
// false.

void Compiler::exprOr() {
  emitOpcode(OP_OR);
  int orpatch = emitShort(0xffff); //< Will be patched.
  skipNewLines();
  parsePrecedence(PREC_LOGICAL_OR);
  patchJump(orpatch);
}

void Compiler::exprAnd() {
  emitOpcode(OP_AND);
  int andpatch = emitShort(0xffff); //< Will be patched.
  skipNewLines();
  parsePrecedence(PREC_LOGICAL_AND);
  patchJump(andpatch);
}

void Compiler::exprBinaryOp() {
  _TokenType op = this->parser.previous.type;
  skipNewLines();
  parsePrecedence((Precedence)(getRule(op)->precedence + 1));

  // Emits the opcode and 0 (means false) as inplace operation.
#define EMIT_BINARY_OP_INPLACE(opcode)\
  do { emitOpcode(opcode); emitByte(0); } while (false)

  switch (op) {
    case TK_DOTDOT:   emitOpcode(OP_RANGE);        break;
    case TK_PERCENT:  EMIT_BINARY_OP_INPLACE(OP_MOD);        break;
    case TK_PLUS:     EMIT_BINARY_OP_INPLACE(OP_ADD);        break;
    case TK_MINUS:    EMIT_BINARY_OP_INPLACE(OP_SUBTRACT);   break;
    case TK_STAR:     EMIT_BINARY_OP_INPLACE(OP_MULTIPLY);   break;
    case TK_FSLASH:   EMIT_BINARY_OP_INPLACE(OP_DIVIDE);     break;
    case TK_STARSTAR: EMIT_BINARY_OP_INPLACE(OP_EXPONENT);   break;
    case TK_AMP:      EMIT_BINARY_OP_INPLACE(OP_BIT_AND);    break;
    case TK_PIPE:     EMIT_BINARY_OP_INPLACE(OP_BIT_OR);     break;
    case TK_CARET:    EMIT_BINARY_OP_INPLACE(OP_BIT_XOR);    break;
    case TK_SRIGHT:   EMIT_BINARY_OP_INPLACE(OP_BIT_RSHIFT); break;
    case TK_SLEFT:    EMIT_BINARY_OP_INPLACE(OP_BIT_LSHIFT); break;
#undef EMIT_BINARY_OP_INPLACE

    case TK_GT:      emitOpcode(OP_GT);    break;
    case TK_LT:      emitOpcode(OP_LT);    break;
    case TK_EQEQ:    emitOpcode(OP_EQEQ);  break;
    case TK_NOTEQ:   emitOpcode(OP_NOTEQ); break;
    case TK_GTEQ:    emitOpcode(OP_GTEQ);  break;
    case TK_LTEQ:    emitOpcode(OP_LTEQ);  break;
    case TK_IN:      emitOpcode(OP_IN);    break;
    case TK_IS:      emitOpcode(OP_IS);    break;
    default:
      UNREACHABLE();
  }
}

void Compiler::exprUnaryOp() {
  _TokenType op = this->parser.previous.type;
  skipNewLines();
  parsePrecedence((Precedence)(PREC_UNARY + 1));

  switch (op) {
    case TK_TILD:  emitOpcode(OP_BIT_NOT); break;
    case TK_PLUS:  emitOpcode(OP_POSITIVE); break;
    case TK_MINUS: emitOpcode(OP_NEGATIVE); break;
    case TK_NOT:   emitOpcode(OP_NOT); break;
    default:
      UNREACHABLE();
  }
}

void Compiler::exprGrouping() {
  skipNewLines();
  compileExpression();
  skipNewLines();
  consume(TK_RPARAN, "Expected ')' after expression.");
}

void Compiler::exprList() {

  emitOpcode(OP_PUSH_LIST);
  int size_index = emitShort(0);

  int size = 0;
  do {
    skipNewLines();
    if (peek() == TK_RBRACKET) break;

    compileExpression();
    emitOpcode(OP_LIST_APPEND);
    size++;

    skipNewLines();
  } while (match(TK_COMMA));

  skipNewLines();
  consume(TK_RBRACKET, "Expected ']' after list elements.");

  patchListSize(size_index, size);
}

void Compiler::exprMap() {
  emitOpcode(OP_PUSH_MAP);

  do {
    skipNewLines();
    if (peek() == TK_RBRACE) break;

    compileExpression();
    consume(TK_COLLON, "Expected ':' after map's key.");
    compileExpression();

    emitOpcode(OP_MAP_INSERT);

    skipNewLines();
  } while (match(TK_COMMA));

  skipNewLines();
  consume(TK_RBRACE, "Expected '}' after map elements.");
}

void Compiler::exprCall() {
  _compileCall(OP_CALL, -1);
}

void Compiler::exprAttrib() {
  consume(TK_NAME, "Expected an attribute name after '.'.");
  const char* name = this->parser.previous.start;
  int length = this->parser.previous.length;

  // Store the name in module's names buffer.
  int index = 0;
  moduleAddString(this->module, this->parser.vm,
                  {name, (size_t)length}, &index);

  // Check if it's a method call.
  if (match(TK_LPARAN)) {
    _compileCall(OP_METHOD_CALL, index);
    return;
  }

  // Check if it's a method call without paranthese.
  if (_compileOptionalParanCall(index)) return;

  if (this->l_value && matchAssignment()) {
    _TokenType assignment = this->parser.previous.type;
    skipNewLines();

    if (assignment != TK_EQ) {
      emitOpcode(OP_GET_ATTRIB_KEEP);
      emitShort(index);
      compileExpression();
      emitAssignedOp(assignment);
    } else {
      compileExpression();
    }

    emitOpcode(OP_SET_ATTRIB);
    emitShort(index);

  } else {
    emitOpcode(OP_GET_ATTRIB);
    emitShort(index);
  }
}

void Compiler::exprSubscript() {
  compileExpression();
  consume(TK_RBRACKET, "Expected ']' after subscription ends.");

  if (this->l_value && matchAssignment()) {
    _TokenType assignment = this->parser.previous.type;
    skipNewLines();

    if (assignment != TK_EQ) {
      emitOpcode(OP_GET_SUBSCRIPT_KEEP);
      compileExpression();
      emitAssignedOp(assignment);

    } else {
      compileExpression();
    }

    emitOpcode(OP_SET_SUBSCRIPT);

  } else {
    emitOpcode(OP_GET_SUBSCRIPT);
  }
}

void Compiler::exprValue() {
  _TokenType op = this->parser.previous.type;
  switch (op) {
    case TK_NULL:  emitOpcode(OP_PUSH_NULL);  break;
    case TK_TRUE:  emitOpcode(OP_PUSH_TRUE);  break;
    case TK_FALSE: emitOpcode(OP_PUSH_FALSE); break;
    default:
      UNREACHABLE();
  }
}

void Compiler::exprSelf() {

  if (this->func->type == FUNC_CONSTRUCTOR ||
      this->func->type == FUNC_METHOD) {
    emitOpcode(OP_PUSH_SELF);
    return;
  }

  // If we reach here 'self' is used in either non method or a closure
  // inside a method.

  if (!this->parser.parsing_class) {
    semanticError(this->parser.previous,
                  "Invalid use of 'self'.");
  } else {
    // FIXME:
    semanticError(this->parser.previous,
                  "TODO: Closures cannot capture 'self' for now.");
  }
}

void Compiler::exprSuper() {

  if (this->func->type != FUNC_CONSTRUCTOR &&
      this->func->type != FUNC_METHOD) {
    semanticError(this->parser.previous,
                  "Invalid use of 'super'.");
    return;
  }

  ASSERT(this->func->ptr != NULL, OOPS);

  int index = 0;
  const char* name = this->func->ptr->name.data();
  int name_length = -1;

  if (!match(TK_LPARAN)) { // super.method().
    consume(TK_DOT, "Invalid use of 'super'.");

    consume(TK_NAME, "Expected a method name after 'super'.");
    name = this->parser.previous.start;
    name_length = this->parser.previous.length;

    consume(TK_LPARAN, "Expected symbol '('.");

  } else { // super().
    name_length = (int)strlen(name);
  }

  if (this->parser.has_syntax_error) return;

  emitOpcode(OP_PUSH_SELF);
  moduleAddString(this->module, this->parser.vm,
                  {name, (size_t)name_length}, &index);
  _compileCall(OP_SUPER_CALL, index);
}

void Compiler::parsePrecedence(Precedence precedence) {
  lexToken();
  if (this->parser.has_syntax_error) return;

  GrammarFn prefix = getRule(this->parser.previous.type)->prefix;

  if (prefix == NULL) {
    syntaxError(this->parser.previous,
                "Expected an expression.");
    return;
  }

  // Make a "backup" of the l value before parsing next operators to
  // reset once it done.
  bool l_value = this->l_value;

  // Inside an expression no new difinition is allowed. We make a "backup"
  // here to prevent such and reset it once we're done.
  bool can_define = this->can_define;
  if (prefix != &Compiler::exprName) this->can_define = false;

  this->l_value = precedence <= PREC_LOWEST;
  (this->*prefix)();

  // Prefix expression can be either allow or not allow a definition however
  // an infix expression can never be a definition.
  this->can_define = false;

  // The above expression cannot be a call '(', since call is an infix
  // operator. But could be true (ex: x = f()). we set is_last_call to false
  // here and if the next infix operator is call this will be set to true
  // once the call expression is parsed.
  this->is_last_call = false;

  while (getRule(this->parser.current.type)->precedence >= precedence) {
    lexToken();
    if (this->parser.has_syntax_error) return;

    _TokenType op = this->parser.previous.type;
    GrammarFn infix = getRule(op)->infix;

    (this->*infix)();

    // TK_LPARAN '(' as infix is the call operator.
    this->is_last_call = (op == TK_LPARAN);
  }

  this->l_value = l_value;
  this->can_define = can_define;
}

/*****************************************************************************/
/* COMPILING                                                                 */
/*****************************************************************************/

// Add a variable and return it's index to the context. Assumes that the
// variable name is unique and not defined before in the current scope.
int Compiler::addVariable(const char* name, uint32_t length, int line) {

  // TODO: should I validate the name for pre-defined, etc?

  // Check if maximum variable count is reached.
  bool max_vars_reached = false;
  const char* var_type = ""; // For max variables reached error message.
  if (this->scope_depth == DEPTH_GLOBAL) {
    if (this->module->globals.count >= MAX_VARIABLES) {
      max_vars_reached = true;
      var_type = "globals";
    }
  } else {
    if (this->func->local_count >= MAX_VARIABLES) {
      max_vars_reached = true;
      var_type = "locals";
    }
  }
  if (max_vars_reached) {
    semanticError(this->parser.previous,
            "A module should contain at most %d %s.", MAX_VARIABLES, var_type);
    return -1;
  }

  // Add the variable and return it's index.

  if (this->scope_depth == DEPTH_GLOBAL) {
    return (int)moduleSetGlobal(this->parser.vm, this->module,
                                {name, (size_t)length}, VAR_NULL);
  } else {
    Local* local = &this->func->locals[this->func->local_count];
    local->name = name;
    local->length = length;
    local->depth = this->scope_depth;
    local->is_upvalue = false;
    local->line = line;
    return this->func->local_count++;
  }

  UNREACHABLE();
  return -1;
}

void Compiler::addForward(int instruction, Fn* fn, Token* tkname) {
  if (this->parser.forwards_count == MAX_FORWARD_NAMES) {
    semanticError(*tkname, "A module should contain at most %d "
                 "implicit forward function declarations.", MAX_FORWARD_NAMES);
    return;
  }

  ForwardName* forward = &this->parser.forwards[
                           this->parser.forwards_count++];
  forward->instruction = instruction;
  forward->func = fn;
  forward->tkname = *tkname;
}

// Add a literal constant to module literals and return it's index.
int Compiler::addConstant(Var value) {
  uint32_t index = moduleAddConstant(this->parser.vm,
                                     this->module, value);
  checkMaxConstantsReached(index);
  return (int) index;
}

// Enters inside a block.
void Compiler::enterBlock() {
  this->scope_depth++;
}

// Change the stack size by the [num], if it's positive, the stack will
// grow otherwise it'll shrink.
void Compiler::changeStack(int num) {
  this->func->stack_size += num;

  // If the compiler has error (such as undefined name), that will not popped
  // because of the semantic error but it'll be popped once the expression
  // parsing is done. So it's possible for negative size in error.
  ASSERT(this->parser.has_errors || this->func->stack_size >= 0, OOPS);

  if (this->func->stack_size > _FN->stack_size) {
    _FN->stack_size = this->func->stack_size;
  }
}

// Write instruction to pop all the locals at the current [depth] or higher,
// but it won't change the stack size of locals count because this function
// is called by break/continue statements at the middle of a scope, so we need
// those locals till the scope ends. This will returns the number of locals
// that were popped.
int Compiler::popLocals(int depth) {
  ASSERT(depth > (int)DEPTH_GLOBAL, "Cannot pop global variables.");

  int local = this->func->local_count - 1;
  while (local >= 0 && this->func->locals[local].depth >= depth) {

    // Note: Do not use emitOpcode(OP_POP);
    // Because this function is called at the middle of a scope (break,
    // continue). So we need the pop instruction here but we still need the
    // locals to continue parsing the next statements in the scope. They'll be
    // popped once the scope is ended.

    if (this->func->locals[local].is_upvalue) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }

    local--;
  }
  return (this->func->local_count - 1) - local;
}

// Exits a block.
void Compiler::exitBlock() {
  ASSERT(this->scope_depth > (int)DEPTH_GLOBAL, "Cannot exit toplevel.");

  // Discard all the locals at the current scope.
  int popped = popLocals(this->scope_depth);
  this->func->local_count -= popped;
  this->func->stack_size -= popped;
  this->scope_depth--;
}

void Compiler::pushFunc(Func* fn, Function* func, FuncType type) {
  fn->type = type;
  fn->outer_func = this->func;
  fn->local_count = 0;
  fn->stack_size = 0;
  fn->ptr = func;
  fn->depth = this->scope_depth;
  this->func = fn;
}

void Compiler::popFunc() {
  this->func = this->func->outer_func;
}

/*****************************************************************************/
/* COMPILING (EMIT BYTECODE)                                                 */
/*****************************************************************************/

// Emit a single byte and return it's index.
int Compiler::emitByte(int byte) {

  pkBufferWrite(&_FN->opcodes, this->parser.vm,
                    (uint8_t)byte);
  pkBufferWrite(&_FN->oplines, this->parser.vm,
                   this->parser.previous.line);
  return (int)_FN->opcodes.count - 1;
}

// Emit 2 bytes argument as big indian. return it's starting index.
int Compiler::emitShort(int arg) {
  emitByte((arg >> 8) & 0xff);
  return emitByte(arg & 0xff) - 1;
}

// Emits an instruction and update stack size (variable stack size opcodes
// should be handled).
void Compiler::emitOpcode(Opcode opcode) {
  emitByte((int)opcode);
  // If the opcode is OP_CALL the compiler should change the stack size
  // manually because we don't know that here.
  changeStack(opcode_info[opcode].stack);
}

// Jump back to the start of the loop.
void Compiler::emitLoopJump() {
  emitOpcode(OP_LOOP);
  int offset = (int)_FN->opcodes.count - this->loop->start + 2;
  emitShort(offset);
}

void Compiler::emitAssignedOp(_TokenType assignment) {
  // Emits the opcode and 1 (means true) as inplace operation.
#define EMIT_BINARY_OP_INPLACE(opcode)\
  do { emitOpcode(opcode); emitByte(1); } while (false)

  switch (assignment) {
    case TK_PLUSEQ:   EMIT_BINARY_OP_INPLACE(OP_ADD);        break;
    case TK_MINUSEQ:  EMIT_BINARY_OP_INPLACE(OP_SUBTRACT);   break;
    case TK_STAREQ:   EMIT_BINARY_OP_INPLACE(OP_MULTIPLY);   break;
    case TK_DIVEQ:    EMIT_BINARY_OP_INPLACE(OP_DIVIDE);     break;
    case TK_MODEQ:    EMIT_BINARY_OP_INPLACE(OP_MOD);        break;
    case TK_POWEQ:    EMIT_BINARY_OP_INPLACE(OP_EXPONENT);   break;
    case TK_ANDEQ:    EMIT_BINARY_OP_INPLACE(OP_BIT_AND);    break;
    case TK_OREQ:     EMIT_BINARY_OP_INPLACE(OP_BIT_OR);     break;
    case TK_XOREQ:    EMIT_BINARY_OP_INPLACE(OP_BIT_XOR);    break;
    case TK_SRIGHTEQ: EMIT_BINARY_OP_INPLACE(OP_BIT_RSHIFT); break;
    case TK_SLEFTEQ:  EMIT_BINARY_OP_INPLACE(OP_BIT_LSHIFT); break;
    default:
      UNREACHABLE();
      break;
#undef EMIT_BINARY_OP_INPLACE
  }
}

void Compiler::emitFunctionEnd() {

  // Don't use emitOpcode(OP_RETURN); Because it'll reduce the stack
  // size by -1, (return value will be popped). This return is implictly added
  // by the compiler.
  // Since we're returning from the end of the function, there'll always be a
  // null value at the base of the current call frame the reserved return value
  // slot.
  emitByte(OP_RETURN);

  emitOpcode(OP_END);
}

// Update the jump offset.
void Compiler::patchJump(int addr_index) {
  int offset = (int)_FN->opcodes.count - (addr_index + 2 /*bytes index*/);
  ASSERT(offset < MAX_JUMP, "Too large address offset to jump to.");

  _FN->opcodes.data[addr_index] = (offset >> 8) & 0xff;
  _FN->opcodes.data[addr_index + 1] = offset & 0xff;
}

// Update the size value for OP_PUSH_LIST instruction.
void Compiler::patchListSize(int size_index, int size) {
  _FN->opcodes.data[size_index] = (size >> 8) & 0xff;
  _FN->opcodes.data[size_index + 1] = size & 0xff;
}

void Compiler::patchForward(Fn* fn, int index, int name) {
  fn->opcodes.data[index] = name & 0xff;
}

/*****************************************************************************/
/* COMPILING (PARSE TOPLEVEL)                                                */
/*****************************************************************************/

// Compile a class and return it's index in the module's types buffer.
int Compiler::compileClass() {

  ASSERT(this->scope_depth == DEPTH_GLOBAL, OOPS);

  // Consume the name of the type.
  consume(TK_NAME, "Expected a class name.");
  const char* name = this->parser.previous.start;
  int name_len = this->parser.previous.length;
  int name_line = this->parser.previous.line;

  // Create a new class.
  int cls_index;
  PKVM* _vm = this->parser.vm;
  Class* cls = newClass(_vm, {name, (size_t)name_len},
                        _vm->builtin_classes[PK_OBJECT], this->module,
                        NULL, &cls_index);
  _vm->vmPushTempRef(static_cast<Object*>(cls)); // cls.
  this->parser.parsing_class = true;

  checkMaxConstantsReached(cls_index);

  if (match(TK_IS)) {
    consume(TK_NAME, "Expected a class name to inherit.");
    if (!this->parser.has_syntax_error) {
      exprName(); // Push the super class on the stack.
    }
  } else {
    // Implicitly inherit from 'Object' class.
    emitPushValue(NAME_BUILTIN_TY, (int)PK_OBJECT);
  }

  emitOpcode(OP_CREATE_CLASS);
  emitShort(cls_index);

  skipNewLines();
  if (match(TK_STRING)) {
    Token* str = &this->parser.previous;
    int index = addConstant(str->value);
    String* docstring = moduleGetStringAt(this->module, index);
    cls->docstring = {docstring->data, docstring->length};
  }

  skipNewLines();
  while (!this->parser.has_syntax_error && !match(TK_END)) {

    if (match(TK_EOF)) {
      syntaxError(this->parser.previous,
                  "Unexpected EOF while parsing class.");
      break;
    }

    // At the top level the stack size should be 1 -- the class, before and
    // after compiling the class.
    ASSERT(this->parser.has_errors ||
           this->func->stack_size == 1, OOPS);

    consume(TK_DEF, "Expected method definition.");
    if (this->parser.has_syntax_error) break;

    compileFunction(FUNC_METHOD);
    if (this->parser.has_syntax_error) break;

    // At the top level the stack size should be 1 -- the class, before and
    // after compiling the class.
    ASSERT(this->parser.has_errors ||
           this->func->stack_size == 1, OOPS);

    skipNewLines();
  }

  int global_index = addVariable(name, name_len, name_line);
  emitStoreValue(NAME_GLOBAL_VAR, global_index);
  emitOpcode(OP_POP); // Pop the class.

  this->parser.parsing_class = false;
  _vm->vmPopTempRef(); // cls.

  return cls_index;
}

// Match operator mathod definition. This will match the operator overloading
// method syntax of ruby.
bool Compiler::matchOperatorMethod(const char** name, int* length, int* argc) {
  ASSERT((name != NULL) && (length != NULL) && (argc != NULL), OOPS);
#define _RET(_name, _argc)                       \
  do {                                           \
    *name = _name; *length = (int)strlen(_name); \
    *argc = _argc;                               \
    return true;                                 \
  } while (false)

  if (match(TK_PLUS)) {
    if (match(TK_SELF)) _RET("+self", 0);
    else _RET("+", 1);
  }
  if (match(TK_MINUS)) {
    if (match(TK_SELF)) _RET("-self", 0);
    else _RET("-", 1);
  }
  if (match(TK_TILD)){
    if (match(TK_SELF)) _RET("~self", 0);
    syntaxError(this->parser.previous,
                "Expected keyword self for unary operator definition.");
    return false;
  }
  if (match(TK_NOT)) {
    if (match(TK_SELF)) _RET("!self", 0);
    syntaxError(this->parser.previous,
                "Expected keyword self for unary operator definition.");
    return false;
  }
  if (match(TK_LBRACKET)) {
    if (match(TK_RBRACKET)) {
      if (match(TK_EQ)) _RET("[]=", 2);
      _RET("[]", 1);
    }
    syntaxError(this->parser.previous,
      "Invalid operator method symbol.");
    return false;
  }

  if (match(TK_PLUSEQ))    _RET("+=",  1);
  if (match(TK_MINUSEQ))   _RET("-=",  1);
  if (match(TK_STAR))      _RET("*",   1);
  if (match(TK_STAREQ))    _RET("*=",  1);
  if (match(TK_FSLASH))    _RET("/",   1);
  if (match(TK_STARSTAR))  _RET("**",  1);
  if (match(TK_DIVEQ))     _RET("/=",  1);
  if (match(TK_PERCENT))   _RET("%",   1);
  if (match(TK_MODEQ))     _RET("%=",  1);
  if (match(TK_POWEQ))     _RET("**=", 1);
  if (match(TK_AMP))       _RET("&",   1);
  if (match(TK_ANDEQ))     _RET("&=",  1);
  if (match(TK_PIPE))      _RET("|",   1);
  if (match(TK_OREQ))      _RET("|=",  1);
  if (match(TK_CARET))     _RET("^",   1);
  if (match(TK_XOREQ))     _RET("^=",  1);
  if (match(TK_SLEFT))     _RET("<<",  1);
  if (match(TK_SLEFTEQ))   _RET("<<=", 1);
  if (match(TK_SRIGHT))    _RET(">>",  1);
  if (match(TK_SRIGHTEQ))  _RET(">>=", 1);
  if (match(TK_EQEQ))      _RET("==",  1);
  if (match(TK_GT))        _RET(">",   1);
  if (match(TK_LT))        _RET("<",   1);
  if (match(TK_DOTDOT))    _RET("..",  1);
  if (match(TK_IN))        _RET("in",  1);

  return false;
#undef _RET
}

// Compile a function, if it's a literal function after this call a closure of
// the function will be at the stack top, toplevel functions will be assigned
// to a global variable and popped, and methods will be bind to the class and
// popped.
void Compiler::compileFunction(FuncType fn_type) {

  const char* name = "(?)"; // Setting "(?)" in case of syntax errors.
  int name_length = 3;

  // If it's an operator method the bellow value will set to a positive value
  // (the argc of the method) it requires to throw a compile time error.
  int operator_argc = -2;

  if (fn_type != FUNC_LITERAL) {

    if (match(TK_NAME)) {
      name = this->parser.previous.start;
      name_length = this->parser.previous.length;

    } else if (fn_type == FUNC_METHOD &&
      matchOperatorMethod(&name, &name_length, &operator_argc)) {

    // Check if any error has been set by operator definition.
    } else if (!this->parser.has_syntax_error) {
      syntaxError(this->parser.previous,
                  "Expected a function name.");
    }

  } else {
    name = LITERAL_FN_NAME;
    name_length = (int) strlen(name);
  }

  if (this->parser.has_syntax_error) return;

  // The function will register itself in the owner's constant pool and it's
  // GC root so we don't need to push it to temp references.
  int fn_index;
  Function* func = newFunction(this->parser.vm, {name, (size_t)name_length},
                               this->module, false, NULL, &fn_index);

  func->is_method = (fn_type == FUNC_METHOD || fn_type == FUNC_CONSTRUCTOR);

  checkMaxConstantsReached(fn_index);

  // Only to be used by the toplevle function to define itself on the globals
  // of the module.
  int global_index = -1;

  if (fn_type == FUNC_TOPLEVEL) {
    ASSERT(this->scope_depth == DEPTH_GLOBAL, OOPS);
    int name_line = this->parser.previous.line;
    global_index = addVariable(name, name_length, name_line);
  }

  if (fn_type == FUNC_METHOD && strncmp(name, CTOR_NAME, name_length) == 0) {
    fn_type = FUNC_CONSTRUCTOR;
  }

  Func curr_fn;
  pushFunc(&curr_fn, func, fn_type);

  int argc = 0;
  enterBlock(); // Parameter depth.

  // Parameter list is optional.
  if (match(TK_LPARAN) && !match(TK_RPARAN)) {
    do {
      skipNewLines();

      consume(TK_NAME, "Expected a parameter name.");
      argc++;

      const char* param_name = this->parser.previous.start;
      uint32_t param_len = this->parser.previous.length;

      // TODO: move this to a functions.
      bool predefined = false;
      for (int i = this->func->local_count - 1; i >= 0; i--) {
        Local* local = &this->func->locals[i];
        if (local->length == param_len &&
            strncmp(local->name, param_name, param_len) == 0) {
          predefined = true;
          break;
        }
      }
      if (predefined) {
        semanticError(this->parser.previous,
                      "Multiple definition of a parameter.");
      }

      addVariable(param_name, param_len,
                          this->parser.previous.line);

    } while (match(TK_COMMA));

    consume(TK_RPARAN, "Expected ')' after parameter list.");
  }

  if (operator_argc >= 0 && argc != operator_argc) {
    semanticError(this->parser.previous,
                  "Expected exactly %d parameters.", operator_argc);
  }

  func->arity = argc;
  changeStack(argc);

  skipNewLines();
  if (match(TK_STRING)) {
    Token* str = &this->parser.previous;
    int index = addConstant(str->value);
    String* docstring = moduleGetStringAt(this->module, index);
    func->docstring = {docstring->data, docstring->length};
  }

  compileBlockBody(BLOCK_FUNC);

  if (fn_type == FUNC_CONSTRUCTOR) {
    emitOpcode(OP_PUSH_SELF);
    emitOpcode(OP_RETURN);
  }

  consume(TK_END, "Expected 'end' after function definition end.");
  exitBlock(); // Parameter depth.
  emitFunctionEnd();

#if DUMP_BYTECODE
  // FIXME:
  // Forward patch are pending so we can't dump constant value that
  // needs to be patched.
  //dumpFunctionCode(this->parser.vm, this->func->ptr);
#endif

  popFunc();

  // Note: After the above compilerPopFunc() call, now we're at the outer
  // function of this function, and the bellow emit calls will write to the
  // outer function. If it's a literal function, we need to push a closure
  // of it on the stack.
  emitOpcode(OP_PUSH_CLOSURE);
  emitShort(fn_index);

  // Capture the upvalues when the closure is created.
  for (int i = 0; i < curr_fn.ptr->upvalue_count; i++) {
    emitByte((curr_fn.upvalues[i].is_immediate) ? 1 : 0);
    emitByte(curr_fn.upvalues[i].index);
  }

  if (fn_type == FUNC_TOPLEVEL) {
    emitStoreValue(NAME_GLOBAL_VAR, global_index);
    emitOpcode(OP_POP);

  } else if (fn_type == FUNC_METHOD || fn_type == FUNC_CONSTRUCTOR) {
    // Bind opcode will also pop the method so, we shouldn't do it here.
    emitOpcode(OP_BIND_METHOD);
  }
}

// Finish a block body.
void Compiler::compileBlockBody(BlockType type) {

  enterBlock();

  if (type == BLOCK_IF) {
    consumeStartBlock(TK_THEN);
    skipNewLines();

  } else if (type == BLOCK_ELSE) {
    skipNewLines();

  } else if (type == BLOCK_FUNC) {
    // Function body doesn't require a 'do' or 'then' delimiter to enter.
    skipNewLines();

  } else {
    // For/While loop block body delimiter is 'do'.
    consumeStartBlock(TK_DO);
    skipNewLines();
  }

  _TokenType next = peek();
  while (!(next == TK_END || next == TK_EOF ||
          ((type == BLOCK_IF) && (next == TK_ELSE || next == TK_ELIF)))) {

    compileStatement();
    skipNewLines();

    next = peek();
  }

  exitBlock();
}

// Parse the module path syntax, emit opcode to load module at that path.
// and return the module's name token
//
//   ex: import foo.bar.baz // => "foo/bar/baz"   => return token 'baz'
//       import .qux.lex    // => "./qux/lex"     => return token 'lex'
//       import ^^foo.bar   // => "../../foo/bar" => return token 'bar'
//
// The name start pointer and its length will be written to the parameters.
// For invalid syntax it'll set an error and return an error token.
Token Compiler::compileImportPath() {

  PKVM* vm = this->parser.vm;
  pkByteBuffer buff; // A buffer to write the path string.
  pkBufferInit(&buff);

  if (match(TK_DOT)) {
    pkByteBufferAddString(&buff, vm, "./");

  } else {
    // Consume parent directory syntax.
    while (match(TK_CARET)) {
      pkByteBufferAddString(&buff, vm, "../");
    }
  }

  Token tkmodule = this->parser.makeErrToken();

  // Consume module path.
  do {
    consume(TK_NAME, "Expected a module name");
    if (this->parser.has_syntax_error) break;

    // A '.' consumed, write '/'.
    if (tkmodule.type != TK_ERROR) pkBufferWrite(&buff, vm, (uint8_t) '/');

    tkmodule = this->parser.previous;
    pkByteBufferAddString(&buff, vm,
                          {tkmodule.start, (size_t)tkmodule.length});

  } while (match(TK_DOT));
  pkBufferWrite(&buff, vm, '\0');

  if (this->parser.has_syntax_error) {
    pkBufferClear(&buff, vm);
    return this->parser.makeErrToken();
  }

  // Create constant pool entry for the path string.
  int index = 0;
  moduleAddString(this->module, this->parser.vm,
                  {(const char*)buff.data, buff.count - 1}, &index);

  pkBufferClear(&buff, vm);

  emitOpcode(OP_IMPORT);
  emitShort(index);

  return tkmodule;
}

// import module1 [as alias1 [, module2 [as alias2 ...]]
void Compiler::compileRegularImport() {
  ASSERT(this->scope_depth == DEPTH_GLOBAL, OOPS);

  do {
    Token tkmodule = compileImportPath();
    if (tkmodule.type == TK_ERROR) return; //< Syntax error. Terminate.

    if (match(TK_AS)) {
      consume(TK_NAME, "Expected a name after 'as'.");
      if (this->parser.has_syntax_error) return;
      tkmodule = this->parser.previous;
    }

    // FIXME:
    // Note that for compilerAddVariable for adding global doesn't create
    // a new global variable if it's already exists. But it'll reuse it. So we
    // don't have to check if it's exists (unlike locals) which is an
    // inconsistance behavior IMO. The problem here is that compilerAddVariable
    // will try to initialize the global with VAR_NULL which may not be
    // accceptable in some scenarios,
    int global_index = addVariable(tkmodule.start,
                                           tkmodule.length, tkmodule.line);

    emitStoreGlobal(global_index);
    emitOpcode(OP_POP);

  } while (match(TK_COMMA) && (skipNewLines(), true));

  // Always end the import statement.
  consumeEndStatement();
}

// from module import sym1 [as alias1 [, sym2 [as alias2 ...]]]
void Compiler::compileFromImport() {
  ASSERT(this->scope_depth == DEPTH_GLOBAL, OOPS);

  Token tkmodule = compileImportPath();
  if (tkmodule.type == TK_ERROR) return; //< Syntax error. Terminate.

  // At this point the module would be on the stack before executing the next
  // instruction.
  consume(TK_IMPORT, "Expected keyword 'import'.");
  if (this->parser.has_syntax_error) return;

  do {
    // Consume the symbol name to import from the module.
    consume(TK_NAME, "Expected symbol to import.");
    if (this->parser.has_syntax_error) return;
    Token tkname = this->parser.previous;

    // Add the name of the symbol to the constant pool.
    int name_index = 0;
    moduleAddString(this->module, this->parser.vm,
                    {tkname.start, (size_t)tkname.length}, &name_index);

    // Don't pop the lib since it'll be used for the next entry.
    emitOpcode(OP_GET_ATTRIB_KEEP);
    emitShort(name_index); //< Name of the attrib.

    // Check if it has an alias.
    if (match(TK_AS)) {
      // Consuming it'll update the previous token which would be the name of
      // the binding variable.
      consume(TK_NAME, "Expected a name after 'as'.");
      tkname = this->parser.previous;
    }

    // FIXME: See the same FIXME for compilerAddVariable()
    // compileRegularImport function.
    int global_index = addVariable(tkname.start,
                                           tkname.length, tkname.line);
    emitStoreGlobal(global_index);
    emitOpcode(OP_POP);

  } while (match(TK_COMMA) && (skipNewLines(), true));

  // Done getting all the attributes, now pop the lib from the stack.
  emitOpcode(OP_POP);

  // Always end the import statement.
  consumeEndStatement();
}

// Compiles an expression. An expression will result a value on top of the
// stack.
void Compiler::compileExpression() {
  parsePrecedence(PREC_LOWEST);
}

void Compiler::compileIfStatement(bool elif) {

  skipNewLines();

  bool can_define = this->can_define;
  this->can_define = false;
  compileExpression(); //< Condition.
  this->can_define = can_define;

  emitOpcode(OP_JUMP_IF_NOT);
  int ifpatch = emitShort(0xffff); //< Will be patched.

  compileBlockBody(BLOCK_IF);

  if (match(TK_ELIF)) {
    // Jump pass else.
    emitOpcode(OP_JUMP);
    int exit_jump = emitShort(0xffff); //< Will be patched.

    // if (false) jump here.
    patchJump(ifpatch);

    enterBlock();
    compileIfStatement(true);
    exitBlock();

    patchJump(exit_jump);

  } else if (match(TK_ELSE)) {
    // Jump pass else.
    emitOpcode(OP_JUMP);
    int exit_jump = emitShort(0xffff); //< Will be patched.

    patchJump(ifpatch);
    compileBlockBody(BLOCK_ELSE);
    patchJump(exit_jump);

  } else {
    patchJump(ifpatch);
  }

  // elif will not consume the 'end' keyword as it'll be leaved to be consumed
  // by it's 'if'.
  if (!elif) {
    skipNewLines();
    consume(TK_END, "Expected 'end' after statement end.");
  }
}

void Compiler::compileWhileStatement() {
  Loop loop;
  loop.start = (int)_FN->opcodes.count;
  loop.patch_count = 0;
  loop.outer_loop = this->loop;
  loop.depth = this->scope_depth;
  this->loop = &loop;

  bool can_define = this->can_define;
  this->can_define = false;
  compileExpression(); //< Condition.
  this->can_define = can_define;

  emitOpcode(OP_JUMP_IF_NOT);
  int whilepatch = emitShort(0xffff); //< Will be patched.

  compileBlockBody(BLOCK_LOOP);

  emitLoopJump();
  patchJump(whilepatch);

  // Patch break statement.
  for (int i = 0; i < this->loop->patch_count; i++) {
    patchJump(this->loop->patches[i]);
  }
  this->loop = loop.outer_loop;

  skipNewLines();
  consume(TK_END, "Expected 'end' after statement end.");
}

void Compiler::compileForStatement() {
  enterBlock();
  consume(TK_NAME, "Expected an iterator name.");

  // Unlike functions local variable could shadow a name.
  const char* iter_name = this->parser.previous.start;
  int iter_len = this->parser.previous.length;
  int iter_line = this->parser.previous.line;

  consume(TK_IN, "Expected 'in' after iterator name.");

  // Compile and store sequence.
  addVariable("@Sequence", 9, iter_line); // Sequence
  bool can_define = this->can_define;
  this->can_define = false;
  compileExpression();
  this->can_define = can_define;

  // Add iterator to locals. It's an increasing integer indicating that the
  // current loop is nth starting from 0.
  addVariable("@iterator", 9, iter_line); // Iterator.
  emitOpcode(OP_PUSH_0);

  // Add the iteration value. It'll be updated to each element in an array of
  // each character in a string etc.
  addVariable(iter_name, iter_len, iter_line); // Iter value.
  emitOpcode(OP_PUSH_NULL);

  // Start the iteration, and check if the sequence is iterable.
  emitOpcode(OP_ITER_TEST);

  Loop loop;
  loop.start = (int)_FN->opcodes.count;
  loop.patch_count = 0;
  loop.outer_loop = this->loop;
  loop.depth = this->scope_depth;
  this->loop = &loop;

  // Compile next iteration.
  emitOpcode(OP_ITER);
  int forpatch = emitShort(0xffff);

  compileBlockBody(BLOCK_LOOP);

  emitLoopJump(); //< Loop back to iteration.
  patchJump(forpatch); //< Patch exit iteration address.

  // Patch break statement.
  for (int i = 0; i < this->loop->patch_count; i++) {
    patchJump(this->loop->patches[i]);
  }
  this->loop = loop.outer_loop;

  skipNewLines();
  consume(TK_END, "Expected 'end' after statement end.");
  exitBlock(); //< Iterator scope.
}

// Compiles a statement. Assignment could be an assignment statement or a new
// variable declaration, which will be handled.
void Compiler::compileStatement() {

  // is_temporary will be set to true if the statement is an temporary
  // expression, it'll used to be pop from the stack.
  bool is_temporary = false;

  // This will be set to true if the statement is an expression. It'll used to
  // print it's value when running in REPL mode.
  bool is_expression = false;

  if (match(TK_BREAK)) {
    if (this->loop == NULL) {
      syntaxError(this->parser.previous,
                  "Cannot use 'break' outside a loop.");
      return;
    }

    ASSERT(this->loop->patch_count < MAX_BREAK_PATCH,
      "Too many break statements (" STRINGIFY(MAX_BREAK_PATCH) ")." );

    consumeEndStatement();
    // Pop all the locals at the loop's body depth.
    popLocals(this->loop->depth + 1);

    emitOpcode(OP_JUMP);
    int patch = emitShort(0xffff); //< Will be patched.
    this->loop->patches[this->loop->patch_count++] = patch;

  } else if (match(TK_CONTINUE)) {
    if (this->loop == NULL) {
      syntaxError(this->parser.previous,
                  "Cannot use 'continue' outside a loop.");
      return;
    }

    consumeEndStatement();
    // Pop all the locals at the loop's body depth.
    popLocals(this->loop->depth + 1);

    emitLoopJump();

  } else if (match(TK_RETURN)) {

    if (this->scope_depth == DEPTH_GLOBAL) {
      syntaxError(this->parser.previous,
                  "Invalid 'return' outside a function.");
      return;
    }

    if (matchEndStatement()) {

      // Constructors will return self.
      if (this->func->type == FUNC_CONSTRUCTOR) {
        emitOpcode(OP_PUSH_SELF);
      } else {
        emitOpcode(OP_PUSH_NULL);
      }

      emitOpcode(OP_RETURN);

    } else {

      if (this->func->type == FUNC_CONSTRUCTOR) {
        syntaxError(this->parser.previous,
                    "Cannor 'return' a value from constructor.");
      }

      bool can_define = this->can_define;
      this->can_define = false;
      compileExpression(); //< Return value is at stack top.
      this->can_define = can_define;

      // If the last expression parsed with compileExpression() is a call
      // is_last_call would be true by now.
      if (this->is_last_call) {
        // Tail call optimization disabled at debug mode.
        if (this->options && !this->options->debug()) {
          ASSERT(_FN->opcodes.count >= 2, OOPS); // OP_CALL, argc
          ASSERT(_FN->opcodes.data[_FN->opcodes.count - 2] == OP_CALL, OOPS);
          _FN->opcodes.data[_FN->opcodes.count - 2] = OP_TAIL_CALL;
        }
      }

      consumeEndStatement();
      emitOpcode(OP_RETURN);
    }
  } else if (match(TK_IF)) {
    compileIfStatement(false);

  } else if (match(TK_WHILE)) {
    compileWhileStatement();

  } else if (match(TK_FOR)) {
    compileForStatement();

  } else {
    this->new_local = false;
    compileExpression();
    consumeEndStatement();

    is_expression = true;
    if (!this->new_local) is_temporary = true;

    this->new_local = false;
  }

  // If running REPL mode, print the expression's evaluated value.
  if (this->options && this->options->replMode() &&
      this->func->ptr == this->module->body->fn &&
      is_expression /*&& this->scope_depth == DEPTH_GLOBAL*/) {
    emitOpcode(OP_REPL_PRINT);
  }

  if (is_temporary) emitOpcode(OP_POP);
}

// Compile statements that are only valid at the top level of the module. Such
// as import statement, function define, and if we're running REPL mode top
// level expression's evaluated value will be printed.
void Compiler::compileTopLevelStatement() {

  // At the top level the stack size should be 0, before and after compiling
  // a top level statement, since there aren't any locals at the top level.
  ASSERT(this->parser.has_errors || this->func->stack_size == 0, OOPS);

  if (match(TK_CLASS)) {
    compileClass();

  } else if (match(TK_DEF)) {
    compileFunction(FUNC_TOPLEVEL);

  } else if (match(TK_IMPORT)) {
    compileRegularImport();

  } else if (match(TK_FROM)) {
    compileFromImport();

  } else {
    compileStatement();
  }

  // At the top level the stack size should be 0, before and after compiling
  // a top level statement, since there aren't any locals at the top level.
  ASSERT(this->parser.has_errors || this->func->stack_size == 0, OOPS);

}

PkResult compile(PKVM* vm, Module* module, std::string_view source,
                 const CompileOptions* options) {

  ASSERT(module != NULL, OOPS);

  // Skip utf8 BOM if there is any.
  const char* src = source.data();
  if (strncmp(src, "\xEF\xBB\xBF", 3) == 0) src += 3;

  Compiler _compiler;
  Compiler* compiler = &_compiler; //< Compiler pointer for quick access.
  compiler->init(vm, src, module, options);

  // If compiling for an imported module the vm->compiler would be the compiler
  // of the module that imported this module. Add the all the compilers into a
  // link list.
  compiler->next_compiler = vm->compiler;
  vm->compiler = compiler;

  // If the module doesn't has a body by default, it's probably was created by
  // the native api function (pkNewModule() that'll return a module without a
  // main function) so just create and add the function here.
  if (module->body == NULL) moduleAddMain(vm, module);
  ASSERT(module->body != NULL, OOPS);

  // If we're compiling for a module that was already compiled (when running
  // REPL or evaluating an expression) we don't need the old main anymore.
  // just use the globals and functions of the module and use a new body func.
  pkBufferClear(&module->body->fn->fn->opcodes, vm);

  // Remember the count of constants, names, and globals, If the compilation
  // failed discard all of them and roll back.
  uint32_t constants_count = module->constants.count;
  uint32_t globals_count = module->globals.count;

  Func curr_fn;
  compiler->pushFunc(&curr_fn, module->body->fn, FUNC_MAIN);

  // Lex initial tokens. current <-- next.
  compiler->lexToken();
  compiler->lexToken();
  compiler->skipNewLines();

  while (!compiler->match(TK_EOF) && !compiler->parser.has_syntax_error) {
    compiler->compileTopLevelStatement();
    compiler->skipNewLines();
  }

  compiler->emitFunctionEnd();

  // Resolve forward names (function names that are used before defined).
  if (!compiler->parser.has_syntax_error) {
    for (int i = 0; i < compiler->parser.forwards_count; i++) {
      ForwardName* forward = &compiler->parser.forwards[i];
      const char* name = forward->tkname.start;
      int length = forward->tkname.length;
      int index = moduleGetGlobalIndex(compiler->module,
                                       {name, (size_t)length});
      if (index != -1) {
        compiler->patchForward(forward->func, forward->instruction, index);
      } else {
        // need_more_lines is only true for unexpected EOF errors. For syntax
        // errors it'll be false by now but. Here it's a semantic errors, so
        // we're overriding it to false.
        compiler->parser.need_more_lines = false;
        compiler->resolveError(forward->tkname, "Name '%.*s' is not defined.",
                     length, name);
      }
    }
  }

  vm->compiler = compiler->next_compiler;

  // If compilation failed, discard all the invalid functions and globals.
  if (compiler->parser.has_errors) {
    module->constants.count = constants_count;
    module->globals.count = module->global_names.count = globals_count;
  }
#if DUMP_BYTECODE
  else {
    // If there is any syntax errors we cannot dump the bytecode
    // (otherwise it'll crash with assertion).
    dumpFunctionCode(compiler->parser.vm, module->body->fn);
    DEBUG_BREAK();
  }
#endif

  // Return the compilation result.
  if (compiler->parser.has_errors) {
    if (compiler->parser.repl_mode && compiler->parser.need_more_lines) {
      return PK_RESULT_UNEXPECTED_EOF;
    }
    return PK_RESULT_COMPILE_ERROR;
  }
  return PK_RESULT_SUCCESS;
}

void compilerMarkObjects(PKVM* vm, Compiler* compiler) {

  // Mark the module which is currently being compiled.
  markObject(vm, static_cast<Object*>(compiler->module));

  // Mark the string literals (they haven't added to the module's literal
  // buffer yet).
  markValue(vm, compiler->parser.current.value);
  markValue(vm, compiler->parser.previous.value);
  markValue(vm, compiler->parser.next.value);

  if (compiler->next_compiler != NULL) {
    compilerMarkObjects(vm, compiler->next_compiler);
  }
}
