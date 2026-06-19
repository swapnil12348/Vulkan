---
name: cpp-best-practices
description: >
  Expert guidance for C++ code review, debugging, and best practices. Triggers when
  the user asks for help with C++ code, wants to audit or review C++ for correctness,
  asks about modern C++ idioms, is debugging a C++ build or runtime issue, or wants
  to understand C++ footguns, undefined behavior, or safety pitfalls. Use this skill
  whenever C++ code, a .cpp/.hpp/.h file, or a C++-related error is the subject of
  the conversation — even if the user just says "help me with my C++ project" or
  "why is my C++ code crashing."
---

# C++ Best Practices

This skill encodes deep institutional knowledge about C++'s footguns, common
misunderstandings, and idiomatic modern practices. Use it to give precise, honest,
actionable guidance.

---

## Core Philosophy

C++ simultaneously rewards and punishes expertise. Most bugs come not from
misunderstanding an algorithm, but from subtle language semantics: invisible copies,
uninitialized memory, silent implicit conversions, and undefined behavior that compiles
without warning. When reviewing or debugging C++, always scan for these first.

---

## Modern C++ Version Reference

| Standard | Key additions |
|----------|--------------|
| C++11    | `auto`, smart pointers, move semantics, lambdas, `constexpr`, range-for, `nullptr` |
| C++14    | Generic lambdas, `make_unique`, `[[deprecated]]` |
| C++17    | Structured bindings, `if constexpr`, `std::optional`, `std::variant`, `std::string_view`, `std::filesystem`, fold expressions, inline variables, `std::scoped_lock`, `insert_or_assign` |
| C++20    | Concepts, ranges, coroutines, `std::span`, `std::format`, modules (early), designated initializers, `<=>` (three-way comparison), `starts_with`/`ends_with`/`contains`, `std::jthread`, `std::erase`/`std::erase_if` |
| C++23    | `std::expected`, `std::print`, `std::mdspan`, `import std;`, deducing `this`, `std::to_underlying` |

Industry standard as of 2025 is roughly **C++17**; C++20 features are increasingly safe
to use in new projects. Avoid C++03/98 patterns in new code unless the codebase mandates
them.

---

## High-Priority Footguns (check these first)

### 1. Uninitialized Variables
```cpp
int x;          // DANGER: garbage value, not zero
int* p;         // DANGER: garbage address, not nullptr
```
Always initialize. Numeric types default to zero only in specific contexts (static
storage, value-initialization with `{}`). POD member variables do **not** auto-initialize.

### 2. Implicit Conversions
C++ silently converts between types in ways that corrupt data:
```cpp
int64_t id = 8'000'000'000LL;
int32_t stored = id;   // truncated silently, compiles without warning by default
```
Enable `-Wall -Wextra -Wshadow -Wconversion` (GCC/Clang) or `/W4 /WX` (MSVC). Flag
any narrowing conversion as a probable bug.

String literals implicitly convert to `bool`, not `std::string`:
```cpp
void process(std::string s);
void process(bool b);
process("hello");  // calls process(bool) — NOT process(std::string)!
```
This is a frequent, hard-to-spot bug. Prefer `explicit` on single-arg constructors.

### 3. Constructors Should Be `explicit` by Default
```cpp
// BAD
struct Seconds { Seconds(int n) {} };
void wait(Seconds s);
wait(5);   // silently converts int → Seconds

// GOOD
struct Seconds { explicit Seconds(int n) {} };
wait(Seconds(5));  // forced explicit call
```

### 4. `vector<bool>` Is Not a Vector
`std::vector<bool>` is a space-optimized bitfield specialization, not a true container.
Its elements are proxy objects; you cannot take the address of an element. Use
`std::vector<char>` or `std::deque<bool>` when you need a real boolean container.

### 5. Iterator Invalidation
Appending to a `std::vector` can reallocate, invalidating **all** existing iterators,
pointers, and references into that vector. Never iterate and append simultaneously:
```cpp
for (auto it = v.begin(); it != v.end(); ++it) {
    v.push_back(*it);   // UB: iterators invalidated on reallocation
}
```

### 6. Use-After-Move
After `std::move`, the source object is in a "valid but unspecified" state. Using it
is almost always a bug:
```cpp
auto s = std::string{"hello"};
auto t = std::move(s);
std::cout << s;   // s may be empty or garbage — don't do this
```

### 7. `std::map` / `std::set` vs. `std::unordered_*`
- `std::map` / `std::set` → balanced BST, **O(log n)** — often wrongly assumed to be O(1)
- `std::unordered_map` / `std::unordered_set` → hash table, **O(1)** amortized, but
  cache-unfriendly (linked-list bucket chaining). For performance-critical code prefer
  `absl::flat_hash_map` (Abseil) or `folly::F14FastMap`.

### 8. `operator[]` on `std::unordered_map` Inserts
```cpp
std::unordered_map<std::string, int> m;
auto val = m["missing_key"];   // inserts default value 0 — silent side effect!
```
Use `.at()` (throws on miss) or `.find()` / `.contains()` when you only want to check.

The reverse surprise: `m.insert({key, val})` does **nothing** if `key` already exists —
it does not overwrite. Use `m[key] = val` to overwrite, or `m.insert_or_assign(key, val)`
(C++17) to make the overwrite-or-insert intent explicit.

### 9. Dangling References / String Views
`std::string_view` and `std::span` are non-owning. Never store them past the lifetime
of their source:
```cpp
std::string_view sv = std::string{"temporary"};  // dangling immediately
```

### 10. Virtual Destructor Rule
Any class with `virtual` methods must have a `virtual` destructor, or deleting through
a base pointer leaks derived-class resources:
```cpp
class Base { virtual void foo(); };          // DANGER: missing virtual ~Base()
class Derived : public Base { ~Derived(); };
Base* p = new Derived();
delete p;  // ~Derived() never called
```

### 11. Object Slicing
Assigning a derived object to a base object **by value** silently strips off the
derived portion — no error, no warning:
```cpp
class Base    { public: int x = 1; virtual void f() {} };
class Derived : public Base { public: int y = 2; void f() override {} };

void process(Base b);  // by-value parameter — slices any Derived passed in

Derived d;
Base b = d;     // sliced: b.x == 1; b.y and the override are gone
process(d);     // also sliced — process only ever sees a Base
```
Pass polymorphic types by reference or pointer (`Base&`, `Base*`, or
`std::unique_ptr<Base>`), never by value.

### 12. `override` Is Optional — Use It Anyway
`override` doesn't change runtime behavior, but it tells the compiler "this must
override a base virtual function" and errors if it doesn't:
```cpp
class Base    { virtual void onUpdate(int dt); };
class Derived : public Base {
    void onUpdate(float dt) override;  // ERROR: doesn't match Base — caught immediately
};
```
Without `override`, `Derived::onUpdate(float)` silently becomes a *new*, unrelated
function instead of an override, and `Base::onUpdate(int)` is still what runs through a
`Base*`. This is a common source of "I changed the base class signature and nothing
happened" bugs.

### 13. Member Initialization Order Follows Declaration Order
Members are initialized in the order they're **declared in the class**, regardless of
the order they appear in the constructor's initializer list:
```cpp
class Widget {
    int b;
    int a;
public:
    Widget(int v) : a(v), b(a * 2) {}  // looks like 'a' is set first...
    // ...but 'b' is declared first, so b(a * 2) runs BEFORE a(v).
    // 'b' is initialized using an uninitialized 'a'. UB.
};
```
`-Wreorder` (in `-Wextra`) catches the mismatch between initializer-list order and
declaration order. Declare members in dependency order, or move dependent computation
into the constructor body where ordering is explicit.

### 14. The "Most Vexing Parse"
```cpp
class Logger { public: Logger() {} void write(const std::string&); };

Logger log();      // NOT a default-constructed Logger — this is a FUNCTION
                    // DECLARATION: 'log' takes no args, returns a Logger.
log.write("hi");   // compile error: 'log' is a function, not an object
```
Any declaration that *could* be parsed as a function declaration is parsed as one.
Brace initialization, `T x{}`, has no such ambiguity and is unaffected — one more
reason to prefer it for default construction.

### 15. Switch Statement Fallthrough
Cases fall through by default unless you `break`:
```cpp
switch (mode) {
    case Mode::Read:
        openForReading();   // falls through to Write if no break!
    case Mode::Write:
        openForWriting();
        break;
}
```
A missing `break` is one of the most common copy-paste bugs in C++. If fallthrough is
*intentional*, mark it explicitly with `[[fallthrough]];` (C++17) so both readers and
the compiler know it's deliberate.

### 16. Signed/Unsigned Integer Comparisons
Comparing a signed and an unsigned integer converts the signed operand to unsigned — a
negative number becomes a very large positive one:
```cpp
int x = -1;
size_t n = 5;          // size_t is unsigned
if (x < n) { ... }     // FALSE: x is converted to SIZE_MAX, not -1
```
Enable `-Wsign-compare` (included in `-Wextra`) to catch these at compile time. Prefer
`size_t` for indices that are never negative, or use range-based `for` / iterators to
avoid the comparison entirely.

### 17. The Static Initialization Order Fiasco
The order in which global/namespace-scope objects in **different translation units**
are initialized is unspecified. If one global's constructor depends on another global
defined in a different `.cpp` file, you may read it before it's constructed:
```cpp
// a.cpp
Config g_config = loadConfig();

// b.cpp
Logger g_logger(g_config);  // g_config may not be constructed yet!
```
Fix with the **construct-on-first-use** idiom: wrap the object in a function-local
`static`, which is guaranteed to initialize on first call (and is thread-safe since
C++11):
```cpp
Config& config() {
    static Config instance = loadConfig();
    return instance;
}
```

### 18. Struct Padding and Member Layout
The compiler inserts padding bytes between members to satisfy alignment requirements.
Declaration order directly affects struct size:
```cpp
// Wastes space: 7 bytes padding after c, 4 bytes after i
struct Bloated {
    char   c;  // 1 byte + 7 pad
    double d;  // 8 bytes
    int    i;  // 4 bytes + 4 pad
};  // sizeof == 24

// Optimal: largest members first, no wasted padding
struct Compact {
    double d;  // 8 bytes
    int    i;  // 4 bytes
    char   c;  // 1 byte + 3 pad
};  // sizeof == 16
```
Sort members from largest to smallest alignment to minimize padding. Use
`static_assert(sizeof(MyStruct) == expected, "layout changed");` to catch accidental
regressions in packed or serialized structs.

### 19. Immediately-Invoked Lambda Expressions (IILE)
A `()` after a lambda's closing `}` invokes it in-place. Easy to miss in dense code:
```cpp
// Is this storing a lambda, or storing the *result* of calling it?
auto result = [&]() { return compute(); }();
//                                        ^^ invokes immediately — result holds int, not lambda

// Make intent explicit:
auto fn = [&]() { return compute(); };
auto result = fn();
```
Scan for `}()` and `}(args)` patterns in code review. Accidental IILEs are a common
source of "why does this run at startup?" bugs in global/static initializers.

### 20. Multiple Inheritance and the Diamond Problem
When two base classes share a common ancestor, a derived class inheriting both gets
**two copies** of the grandparent, leading to ambiguity:
```cpp
struct Animal { virtual void speak(); };
struct Dog    : Animal {};
struct Robot  : Animal {};
struct RoboDog : Dog, Robot {};  // two Animal subobjects — diamond

RoboDog r;
r.speak();  // ERROR: ambiguous — Dog::Animal::speak or Robot::Animal::speak?
```
Use `virtual` inheritance to share a single grandparent subobject:
```cpp
struct Dog   : virtual Animal {};
struct Robot : virtual Animal {};
struct RoboDog : Dog, Robot {};  // now only one Animal subobject
```
Virtual inheritance adds overhead and complicates construction (the most-derived class
must directly initialize the virtual base). **Prefer composition over multiple
inheritance** whenever possible.

### 21. Reserved Names — Never Prefix Identifiers with Underscore
Names beginning with an underscore followed by an uppercase letter (`_Foo`) or
containing double underscores anywhere (`__foo`, `my__val`) are reserved for the
compiler and standard library **everywhere**. Names beginning with a single underscore
(`_count`) are reserved in the global namespace:
```cpp
int _count = 0;      // DANGER: reserved at global scope
int __value = 0;     // DANGER: reserved everywhere
class _MyClass {};   // DANGER: reserved everywhere
```
Never use these patterns for your own identifiers. A common alternative for private
member variables is a **trailing underscore**: `count_`, `value_` — it's clear,
collision-free, and widely used in Google/Abseil-style codebases.

### 22. `std::remove` Doesn't Remove — The Erase-Remove Idiom
`std::remove` and `std::remove_if` do **not** erase elements; they move unwanted
elements to the logical end and return an iterator to that position. Container size is
unchanged. You must call `erase` yourself:
```cpp
std::vector<int> v = {1, 2, 3, 2, 4};

// WRONG: v still has 5 elements, just reordered
std::remove(v.begin(), v.end(), 2);

// CORRECT pre-C++20: erase-remove idiom
v.erase(std::remove(v.begin(), v.end(), 2), v.end());

// CORRECT C++20: free-function std::erase (cleaner, one line)
std::erase(v, 2);                     // removes all matching values
std::erase_if(v, [](int x){ return x % 2 == 0; });  // removes by predicate
```
Prefer `std::erase` / `std::erase_if` (C++20) in new code.

### 23. `std::move_if_noexcept`
Containers like `std::vector` fall back to **copying** instead of moving during
reallocation if the element's move constructor is not `noexcept`. This means failing
to mark move constructors `noexcept` can silently destroy performance:
```cpp
// If MyType's move ctor is not noexcept, vector reallocation copies every element
std::vector<MyType> v;
v.push_back(obj);  // may trigger O(n) copies, not O(n) moves
```
In generic code where you want "move if safe, copy otherwise":
```cpp
auto x = std::move_if_noexcept(obj);
```
The simpler rule: **always mark move constructors `noexcept`** when they genuinely
cannot throw. The STL depends on this guarantee.

### 25. Mismatched Container Iterators
Passing `.begin()` from one container and `.end()` from another compiles without
warning but is undefined behavior:
```cpp
std::vector<int> v1 = {3, 1, 4};
std::vector<int> v2 = {1, 5, 9};

std::sort(v1.begin(), v2.end());   // UB: iterators from different containers
std::sort(v1.begin(), v1.end());   // CORRECT

// C++20 ranges eliminate this class of error entirely
std::ranges::sort(v1);             // no begin/end to mix up
```
This is easy to introduce via copy-paste when refactoring code that previously used
a single container. Enable `-fsanitize=address,undefined` to catch these at runtime.
Prefer `std::ranges::` algorithms in new code.

### 24. Aggregate Initialization Is Version-Sensitive
The definition of "aggregate" changed in nearly every C++ version from C++11 onward.
Whether a class can use brace-initialization without a constructor, and whether an
`=` is required, can produce different types or fail silently across standard versions:
```cpp
struct Foo { int x; int y; };
Foo a = {1, 2};  // aggregate init — fine in all versions
Foo b{1, 2};     // aggregate init without = — fine in C++11+

// But add a user-provided constructor in C++11/14 and Foo is no longer an aggregate
// C++17 relaxed the rules (base classes allowed); C++20 relaxed further
```
When maintaining cross-version codebases, test aggregate initialization explicitly.
If the struct has a base class or user-provided constructors, verify which standard
version permits the brace syntax you're using.

---

## Initialization Cheat Sheet

| Syntax | What it does |
|--------|-------------|
| `T x;` | Default-init. POD = garbage. Class = default ctor |
| `T x{};` | Value-init. POD = zero. Class = default ctor |
| `T x{a, b};` | Aggregate/list init. Narrowing conversions are errors |
| `T x = T{a};` | Copy-init via `T{a}` then copy-elision (usually same as above) |
| `auto x = expr;` | Type deduced from `expr`. Copies unless `auto&` or `auto&&` |
| `T x();` | **DANGER**: parsed as a function declaration, not an object — see footgun #14 |

Prefer `T x{}` over `T x` for local variables when zero/default state is intended.

### Designated Initializers (C++20)
Named member initialization that self-documents intent and is immune to reordering
of unrelated fields:
```cpp
struct Config {
    int  timeout = 30;
    bool verbose = false;
    std::string host;
};

Config c{ .timeout = 60, .verbose = true, .host = "localhost" };
// Unspecified members (.host here if omitted) take their default values.
// Members MUST appear in declaration order — out-of-order is ill-formed in C++
// (unlike C99 designated initializers, which allow any order).
```
Use designated initializers for structs with more than two or three fields; they
prevent silent bugs when fields are reordered in the struct definition.

---

## Casting Rules

| Cast | When to use |
|------|-------------|
| `static_cast<T>(x)` | Numeric conversions, up/down casts with known type |
| `dynamic_cast<T*>(p)` | Polymorphic downcasts; returns `nullptr` on failure |
| `reinterpret_cast<T*>(p)` | Bit-level reinterpretation; almost always a code smell |
| `const_cast<T*>(p)` | Remove `const`; UB if original was actually const |
| `std::bit_cast<T>(x)` | Type-punning with defined behavior (C++20) |

Do **not** create aliases or macros for casts — it confuses other developers who
encounter unfamiliar syntax and expect standard C++.
For enums, use `std::to_underlying(e)` (C++23) or `static_cast<int>(e)` — not a
custom alias.

---

## Ownership & Smart Pointers

**Default choice: `std::unique_ptr`**. It expresses single ownership with zero overhead.

`std::shared_ptr` is for multi-owner scenarios where lifetime is genuinely unknown at
compile time (e.g., multiple threads each holding a reference). Using it everywhere is
a code smell — it adds reference-counting overhead and can hide ownership design flaws.

Raw pointers (`T*`) in modern C++ signal *non-owning observers*. They do not manage
lifetime. Do not `delete` a raw pointer obtained from a smart pointer.

**ABI overhead caveat:** `std::unique_ptr` is *not* zero overhead in all calling
conventions. Because it has a non-trivial destructor, the Windows x64 ABI (and
others) cannot pass it in a register — it must be passed on the stack. The same
issue affects `std::string_view` and `std::span` on Windows. In extremely hot code
paths where this matters, pass a raw (non-owning) pointer instead and document the
intent:
```cpp
void render(Widget* w);          // hot path: raw pointer, non-owning, register-passed
void store(std::unique_ptr<Widget> w);  // ownership transfer: unique_ptr is fine
```
Profile before worrying about this — it is rarely the bottleneck.

```cpp
// GOOD: clear ownership
auto widget = std::make_unique<Widget>();
render(widget.get());   // raw pointer = "I don't own this"

// BAD: ambiguous ownership
Widget* widget = new Widget();
```

---

## RAII (Resource Acquisition Is Initialization)

Despite the confusing name (better alternatives: SBRM — Scope-Bound Resource
Management; or CADRE — Constructor Acquires, Destructor Releases), RAII is a simple,
critical idiom:

> **Acquire a resource in a constructor. Release it in the destructor.**

Because destructors run deterministically at scope exit — including on exception,
early return, or `goto` — RAII guarantees cleanup on every code path:

```cpp
// BAD: manual resource management — fclose skipped on exception
void process() {
    FILE* f = fopen("data.txt", "r");
    parseFile(f);   // throws? fclose never called
    fclose(f);
}

// GOOD: RAII wrapper guarantees cleanup
struct FileGuard {
    FILE* f;
    explicit FileGuard(const char* path) : f(fopen(path, "r")) {}
    ~FileGuard() { if (f) fclose(f); }
};

void process() {
    FileGuard g("data.txt");
    parseFile(g.f);   // exception? ~FileGuard() still runs
}

// BETTER: use existing RAII wrappers from the standard library or Abseil
auto f = std::unique_ptr<FILE, decltype(&fclose)>(fopen("data.txt","r"), &fclose);
```

The **Rule of Zero** is RAII taken to its logical conclusion: compose your class from
members that already manage their own resources (`std::unique_ptr`, `std::vector`,
`std::string`, `std::mutex`). The compiler then generates correct RAII behavior for
your class automatically.

---

## Move Semantics Gotchas

- `std::move` does **not** move anything. It casts to `T&&`, enabling move construction.
  A better name would have been `std::rvalue_cast`. Do not mistake it for the
  `std::move` in `<algorithm>` which *actually* moves elements in a range.
- Do **not** `std::move` a return value — it defeats NRVO:
  ```cpp
  std::string make() {
      std::string s = "hello";
      return std::move(s);   // BAD: disables copy elision
      return s;              // GOOD: NRVO applies
  }
  ```
- Mark move constructors `noexcept` wherever possible. Containers like `std::vector`
  use copy-fallback if the move constructor might throw (see footgun #23).
- There are **two** `std::move`s in the standard library: `<utility>` (rvalue cast) and
  `<algorithm>` (actually moves elements in a range). Do not confuse them.
- Use `std::move_if_noexcept` in generic code that should move only when safe (see
  footgun #23).
- C++ uses **non-destructive moves**: after `std::move`, the moved-from object is
  left in a "valid but unspecified" state and its destructor **still runs**. This
  means moves are not truly zero-cost — the moved-from destructor executes on scope
  exit. Rust's *destructive* moves have no such overhead because the compiler proves
  the value is gone and suppresses the destructor entirely. Mark move constructors
  `noexcept` to at least let containers skip the copy fallback (footgun #23).

---

## The Hidden Cost of Copies

C++ copies by default. An innocent `=` or by-value parameter may copy megabytes
without any visible indication at the call site:

```cpp
std::vector<HeavyObject> a = getResults();
std::vector<HeavyObject> b = a;    // deep copy of entire container — invisible
```

**Common invisible copy patterns:**
```cpp
auto x = container;                // copies the whole container; use const auto& or std::move
for (auto item : vec) { ... }      // copies each element; use const auto& or auto&&
void fn(std::string s) { ... }     // copy on every call; prefer const std::string& or std::string&&
```

The fundamental issue: whether `b = a` makes a deep copy, a shallow copy, or moves
depends entirely on the class's copy/assignment implementation — **none of which is
visible at the call site**. You must know the semantics of every type you assign.

**Rules to minimize surprise:**
- Range-for over non-trivial containers: `for (const auto& item : vec)`
- Pass non-trivial types as `const T&` (read-only) or `T&&` (consuming)
- Pass small trivial types (`int`, `float`, pointers) by value — the `const&` is slower
- When transferring ownership, use `std::move` explicitly so the copy is visible

Copies are also the primary source of invisible performance regressions. Always
profile before concluding that a slow path is algorithmic.

---

## Rule of Zero, Three, and Five

If a class manages a resource (memory, file handle, mutex, etc.), it generally needs
all five "special member functions" defined together:

- Destructor
- Copy constructor
- Copy assignment operator
- Move constructor
- Move assignment operator

**Rule of Three** (pre-C++11): if you need any one of {destructor, copy constructor,
copy assignment}, you almost certainly need all three. The compiler-generated copy
operations for a class managing a raw resource will copy the handle, not the resource —
leading to double-frees or use-after-free.

**Rule of Five** (C++11+): the same applies to move constructor and move assignment.
Declaring any one of the five (even just the destructor) suppresses the
compiler-generated move operations, silently falling back to copies.

**Rule of Zero** (preferred): don't write *any* of the five. Compose your class out of
members that already manage themselves — `std::unique_ptr`, `std::vector`,
`std::string`, etc. The compiler-generated special members then do the right thing
automatically via each member's own RAII behavior.

```cpp
// BAD: Rule of Three/Five violation waiting to happen
class RawBuffer {
    char* data;
public:
    RawBuffer(size_t n) : data(new char[n]) {}
    ~RawBuffer() { delete[] data; }
    // no copy/move defined — copying this double-frees `data`
};

// GOOD: Rule of Zero — let std::vector manage the memory
class Buffer {
    std::vector<char> data;
public:
    Buffer(size_t n) : data(n) {}
    // copy, move, destructor all "just work"
};
```

### Copy-and-Swap Idiom
When you genuinely must write a Rule-of-Five class (wrapping a legacy resource,
say), the **copy-and-swap idiom** implements copy assignment in terms of the copy
constructor and a `noexcept` swap — giving strong exception safety with no code
duplication:
```cpp
class RawBuffer {
    char*  data_;
    size_t size_;
public:
    RawBuffer(size_t n) : data_(new char[n]), size_(n) {}
    ~RawBuffer() { delete[] data_; }

    RawBuffer(const RawBuffer& o) : data_(new char[o.size_]), size_(o.size_) {
        std::memcpy(data_, o.data_, size_);
    }
    RawBuffer(RawBuffer&& o) noexcept : data_(o.data_), size_(o.size_) {
        o.data_ = nullptr; o.size_ = 0;
    }

    // Single assignment operator handles both copy and move
    // The parameter is taken by value: copy-ctor or move-ctor runs at the call site
    RawBuffer& operator=(RawBuffer other) noexcept {
        swap(*this, other);   // steal other's guts; other's dtor cleans up the old ones
        return *this;
    }

    friend void swap(RawBuffer& a, RawBuffer& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }
};
```
Prefer the **Rule of Zero** whenever possible — the Copy-and-Swap idiom is for the
rare class that truly manages a raw resource.

---

## `const` Correctness

- Prefer `const` on everything that doesn't need to change. Aim for const-by-default.
- `const` can go on either side of the type: `const int*` == `int const*`. Use the
  "read right-to-left" rule: `int* const p` = "const pointer to int".
- **Never** store `const` member variables or `const` references as class members —
  they break assignment operators.
- `constexpr` functions are `inline` by default; `constexpr` variables are **not**.
- `mutable` lets a specific member be modified even through a `const` object/method —
  the escape hatch for caches, memoization, and internal mutexes
  (`mutable std::mutex m_;`) that don't affect a class's *logical* const-ness. Use it
  sparingly, and only when the mutation is invisible to callers.

---

## The `static` Keyword — Three Unrelated Meanings

1. **Local static** — persists across calls: `static int count = 0;`
2. **Class static** — shared across instances: `static int count;`
3. **File-scope static** — restricts symbol to translation unit (like C's `static`)

These meanings are completely independent. Prefer anonymous namespaces over (3) in
new code for intent clarity.

---

## The `inline` Keyword — Two (Almost Opposite) Meanings

- **`inline` function**: permits the function to be **defined in multiple translation
  units** (e.g., in a header included everywhere) without violating the One Definition
  Rule. The linker collapses the duplicates into one. It *may* also hint the compiler
  to inline the call, but that's now mostly an independent optimizer decision.
- **`inline` variable** (C++17): the *opposite* guarantee — ensures there is exactly
  **one instance** of the variable across the whole program, even though the header
  defining it is included in many `.cpp` files.

```cpp
// header.hpp
inline int globalCounter = 0;              // C++17: exactly one instance program-wide
inline void log(const std::string& msg) {  // OK to define in a header — linker merges
    /* ... */
}
```
Don't confuse "inline this function for performance" (a request, freely ignored by the
compiler) with "inline this variable for ODR" (a hard guarantee).

---

## Templates and Concepts

### Templates Must Generally Live in Headers
The compiler needs the full template definition at every instantiation site. You cannot
define a template in a `.cpp` file and use it from another `.cpp` file — you'll get
linker errors:
```cpp
// WRONG: template definition in .cpp file
// foo.cpp
template <typename T>
void foo(T x) { /* ... */ }  // linker error when used from other translation units

// CORRECT: template definition in header
// foo.hpp
template <typename T>
void foo(T x) { /* ... */ }
```
Exception: **explicit instantiation** — define the template in a `.cpp` file and
explicitly instantiate it for all types you need. Useful for keeping compile times down
when you control all instantiation types.

### SFINAE (Pre-C++20) — Avoid in New Code
SFINAE (Substitution Failure Is Not An Error) is the pre-C++20 technique for
constraining which types a template accepts. It works but produces unreadable code and
catastrophic error messages:
```cpp
// SFINAE: only enables this overload for integral types (C++11/14/17)
template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void process(T val) { /* only for integral types */ }
```

### Concepts (C++20) — The Modern Replacement
Concepts constrain templates directly, produce readable error messages, and document
intent:
```cpp
// Named concept constraint
template <std::integral T>
void process(T val) { /* only for integral types */ }

// Inline requires clause
template <typename T>
requires std::integral<T> && (sizeof(T) >= 4)
void processLarge(T val) {}

// Abbreviated function template (cleanest syntax)
void processAny(std::integral auto val) {}
```
Always prefer concepts over SFINAE in C++20+ codebases.

### `if constexpr` for Compile-Time Branching
Instead of writing SFINAE overloads, use `if constexpr` to branch inside a single
template body:
```cpp
template <typename T>
void serialize(T val) {
    if constexpr (std::is_integral_v<T>) {
        writeInt(val);
    } else if constexpr (std::is_floating_point_v<T>) {
        writeFloat(val);
    } else {
        writeObject(val);
    }
}
```
Only the branch matching the type is compiled. The others are discarded — not just
not-executed, but not compiled at all, so they can reference members that don't exist
on the other types.

### Template Error Messages
Template errors produce long cascades of instantiation context. Clang generally gives
the most readable template errors; GCC and MSVC are noisier. Use concepts to get
errors at the call site rather than inside the template body. If you cannot use
concepts, a `static_assert` at the top of the template body gives a human-readable
message:
```cpp
template <typename T>
void process(T val) {
    static_assert(std::is_integral_v<T>, "process() requires an integral type");
    // ...
}
```

### Template Instantiation Cost
Each unique template instantiation generates code. Heavy template use increases:
- Binary size
- Compile times (especially with complex metaprogramming)
- Debug information size

Use `extern template` to suppress redundant instantiations across translation units
when the same instantiation appears in many `.cpp` files.

### CRTP — Curiously Recurring Template Pattern
Also called *static polymorphism* or *compile-time inheritance*. A base class
template takes the derived class as its own template parameter, enabling
polymorphic-style dispatch with no virtual function overhead:
```cpp
template <typename Derived>
class Shape {
public:
    // Non-virtual dispatch — resolved at compile time
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

class Circle : public Shape<Circle> {
public:
    double radius;
    double area_impl() const { return 3.14159 * radius * radius; }
};

class Square : public Shape<Square> {
public:
    double side;
    double area_impl() const { return side * side; }
};

template <typename Derived>
void printArea(const Shape<Derived>& s) {
    std::cout << s.area() << '\n';  // no virtual dispatch, fully inlined
}
```
Key properties:
- **Zero runtime overhead** — no vtable, no pointer indirection
- Wrong derived type → compile error (can't accidentally slice)
- C++20 **Concepts** replace many CRTP use cases with cleaner syntax; audit existing
  CRTP before adding new instances — a `concept` constraint is usually simpler
- The pattern is also used for **mixin** composition: `class Logger : public
  Timestamper<Logger>, public Formatter<Logger> {}`

---

## `std::variant`, `std::visit`, and `std::monostate`

`std::variant<A, B, C>` is a type-safe discriminated union — it holds exactly one of
its listed types at a time.

### The Overloaded Visitor Pattern
`std::visit` applies a callable to whichever alternative is active. The idiomatic
helper for multi-type visitors:
```cpp
// Boilerplate helper (put in a shared header once)
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;  // C++17 deduction guide

// Usage
std::variant<int, std::string, double> v = "hello";
std::visit(overloaded{
    [](int i)               { std::cout << "int: "    << i;    },
    [](const std::string& s){ std::cout << "string: " << s;    },
    [](double d)            { std::cout << "double: " << d;    }
}, v);
```

### Access Patterns
```cpp
// Check which alternative is active
if (std::holds_alternative<int>(v)) { /* ... */ }

// Safe access — returns pointer, nullptr on wrong type
if (auto* p = std::get_if<int>(&v)) { use(*p); }

// Throwing access — throws std::bad_variant_access on wrong type
int i = std::get<int>(v);
```
Prefer `std::get_if` over `std::get` when you're not certain of the active type.

### `std::monostate` — The Empty Alternative
`std::monostate` is a placeholder type for variants that need a "nothing" state (e.g.,
representing an uninitialized or empty condition). Despite its abstruse name, it simply
means "this variant currently holds nothing meaningful":
```cpp
std::variant<std::monostate, int, std::string> v;  // default: holds monostate
v = 42;       // now holds int
v = "hello";  // now holds string
v = {};       // back to monostate ("empty" state)
```

---

## Operator Overloading Pitfalls

Overload operators only when the meaning is **unambiguous and conventional**.
Operator overloading is invisible at the call site — a reader sees `a + b` and must
know the type to understand what code runs.

**Acceptable:**
- `+`, `-`, `*`, `/` for math types (vectors, matrices, complex numbers)
- `<<`, `>>` for stream I/O
- `==`, `!=`, `<`, `<=>` for comparison/ordering
- `[]` for indexed containers
- `()` for function objects / callables

**Avoid:**
```cpp
// Surprising: / for path concatenation (std::filesystem::path does this)
auto p = basePath / "subdir";  // division symbol used for concatenation

// Catastrophic: overloading operator& (address-of)
struct Bad {
    int* operator&() { return nullptr; }  // now &obj returns nullptr
};
// Use std::addressof(obj) to get the real address of any object
// whose operator& might be overloaded
```
- Never overload `operator&`, `operator,`, or `operator&&` / `operator||` — they lose
  their sequencing guarantees when overloaded.
- Overloading `new` / `delete` is occasionally valid for custom allocators, but almost
  always a maintenance hazard in application code.
- In C++20, defining `operator==` also synthesizes `operator!=` automatically. Define
  `operator<=>` (spaceship) to get all six comparison operators from one definition.

---

## Structured Bindings (C++17)

Prefer structured bindings when unpacking pairs, tuples, or map entries — they name
the parts semantically instead of exposing the cryptic `first`/`second`:
```cpp
std::map<std::string, int> scores;

// BAD: first/second are meaningless
for (const auto& entry : scores) {
    process(entry.first, entry.second);
}

// GOOD: names communicate intent
for (const auto& [name, score] : scores) {
    process(name, score);
}

// Also useful for insert results and structured returns
auto [it, inserted] = m.insert({"key", 42});
if (!inserted) { /* key already existed */ }

// Unpack a struct directly
struct Point { int x, y; };
auto [x, y] = getPoint();
```

---

## Enums, Classes, and Structs: Default Behaviors

- **Prefer `enum class` over plain `enum`**. A plain `enum`'s enumerators leak into the
  enclosing scope and implicitly convert to `int`:
  ```cpp
  enum Color { Red, Blue };
  enum Fruit { Apple, Orange };
  if (Red == Apple) { /* compiles! both are just int 0 */ }

  enum class Direction { Left, Right };
  Direction d = Direction::Left;  // must qualify
  ```
  Use `std::to_underlying(e)` (C++23) or `static_cast<int>(e)` when you need the
  underlying integer.

- **`class` defaults to private inheritance and private members; `struct` defaults to
  public.** `class Derived : Base` is *private* inheritance:
  ```cpp
  class Derived : Base { ... };          // private — Base interface hidden
  class Derived : public Base { ... };   // public — what you usually want
  ```
  Always write `public` explicitly on `class` inheritance.

- **Abstract classes** require `virtual` on every method and `= 0` to make it pure
  virtual. C++ has no `interface` keyword:
  ```cpp
  class ILogger {
  public:
      virtual void log(const std::string& msg) = 0;  // pure virtual
      virtual ~ILogger() = default;                   // MUST have virtual dtor
  };
  ```
  A class with any pure virtual function cannot be instantiated directly.

### Enum-to-String Has No Native Support
C++ provides no built-in way to convert an `enum class` value to its name as a
string. Common workarounds, in increasing order of maintenance risk:

```cpp
// 1. Manual switch — safe but must be updated whenever the enum changes
std::string_view toString(Color c) {
    switch (c) {
        case Color::Red:   return "Red";
        case Color::Green: return "Green";
        case Color::Blue:  return "Blue";
    }
    return "Unknown";
}

// 2. magic_enum (header-only, GitHub: Neargye/magic_enum)
// Uses compiler-extension hacks (__PRETTY_FUNCTION__ / __FUNCSIG__) — not standard,
// but works on GCC, Clang, MSVC. Zero runtime overhead.
#include <magic_enum.hpp>
auto name = magic_enum::enum_name(Color::Red);  // "Red"

// 3. Reflection (C++26, not yet widely available)
```
If you add or rename an enumerator and forget to update a manual `toString`, the
compiler won't warn you. Add a `static_assert` on `enum_count` or use `magic_enum`
to catch this at compile time. Reflection in C++26 will eventually solve this
natively, but industry adoption will lag by years.

---

## Header Files and Include Guards

Every header needs an include guard. Two styles exist; prefer `#pragma once` for
clarity in new code, but add traditional guards for maximum portability:
```cpp
#pragma once
// --- OR ---
#ifndef MY_PROJECT_FOO_HPP
#define MY_PROJECT_FOO_HPP
// ...
#endif
```

**Never** put `using namespace std;` (or any namespace) in a header — it pollutes every
file that includes it.

Private implementation details belong in `.cpp` files, not headers. Use the PIMPL
idiom if you need to hide class internals from the public header.

### Include Ordering
The order of `#include` directives matters when headers define macros that affect
subsequent headers. The most notorious Windows example:
```cpp
// WRONG: windows.h redefines things that winsock2.h needs
#include <windows.h>
#include <winsock2.h>   // many symbols already clobbered

// CORRECT: always include winsock2.h before windows.h
#include <winsock2.h>
#include <windows.h>
```
A common defensive include order: your own module header first, then third-party
headers, then standard library headers. This surfaces problems where your own headers
accidentally depend on things that happen to be included before them.

---

## Macro Hazards

The preprocessor runs before the compiler sees any C++ syntax — macros don't respect
namespaces, scoping, or types:
```cpp
#include <windows.h>   // defines macros: min, max, SendMessage, ...

void SendMessage(const std::string& msg);  // silently rewritten or breaks
int lo = std::min(a, b);                    // can fail to compile — min is a macro
```
- Define `NOMINMAX` and `WIN32_LEAN_AND_MEAN` before including `windows.h`.
- Never `#define` short, common identifiers. Prefix project macros: `MYPROJ_ASSERT`.
- Be especially wary of macros that redefine `private` to `public` for testing — they
  apply to *every* class in every header parsed afterward.
- Macros do not respect namespace boundaries — a macro defined in an included header
  silently affects all subsequent code in the translation unit.

---

## Undefined Behavior — Common Triggers

These compile silently and cause unpredictable crashes or security vulnerabilities:

- Signed integer overflow (`INT_MAX + 1`)
- Out-of-bounds array/vector access via `[]` (use `.at()` in debug builds)
- Dereferencing a null or dangling pointer
- Reading an uninitialized variable
- Using an object after it has been freed or moved-from
- Modifying a string literal
- Violating strict aliasing with pointer casts (use `std::bit_cast` or `memcpy`)
- Left-shifting into or past the sign bit
- Data races — two threads accessing the same memory without synchronization where at
  least one access is a write (UB even for "simple" types like `int`)

Enable sanitizers in CI: `-fsanitize=address,undefined` (Clang/GCC) catches a large
fraction of these at runtime; `-fsanitize=thread` catches data races.

---

## Integer Types & Portability

C++ guarantees only *minimum* sizes for built-in types, not exact ones:

| Type | Typical size |
|------|-------------|
| `int` | 32 bits (almost everywhere) |
| `long` | 32 bits on Windows (even 64-bit); 64 bits on 64-bit Linux/macOS |
| `long long` | at least 64 bits everywhere |
| `size_t` | unsigned; matches pointer width (32 or 64 bits) |

- When exact size matters (file formats, network protocols, serialization), use
  fixed-width types from `<cstdint>`: `int32_t`, `uint64_t`, etc.
- `size_t` (returned by `.size()`, `sizeof`) is **unsigned** — see footgun #16.
- `char` may be signed or unsigned depending on platform/compiler. Use `unsigned char`
  (or `std::byte`) for byte data, and `char` only for text.

---

## String Encoding: `std::string` Is a Byte String

`std::string` stores raw bytes with **no Unicode awareness**:
- `s.size()` returns bytes, not characters (a 3-byte UTF-8 emoji counts as 3)
- `s[i]` returns a byte, not a Unicode code point
- `std::toupper` / `std::tolower` are locale-dependent and wrong for non-ASCII text
- String literals use whatever encoding the compiler assumes (usually UTF-8 on modern
  toolchains, but not guaranteed by the standard)

**Practical implications:**
```cpp
std::string emoji = "😀";       // valid — stores 4 UTF-8 bytes
emoji.size();                    // returns 4, not 1
emoji[0];                        // returns 0xF0, the first UTF-8 byte — not the emoji

// Splitting, reversing, or indexing a UTF-8 string by position is incorrect
// without a Unicode-aware library
```

For real Unicode support, use **ICU**, **Abseil's string utilities**, or the **`{fmt}`**
library. For C++20+, `char8_t` and `u8""` literals signal UTF-8 intent but provide
no higher-level Unicode operations. `std::string` missing `split`, `join`, and `trim`
is intentional — but `starts_with`, `ends_with`, and `contains` were added in C++20.

---

## Standard Library Quick-Reference: Things to Avoid

| Don't use | Use instead | Why |
|-----------|------------|-----|
| `std::vector<bool>` | `std::vector<char>` | Proxy object, not a real container |
| `std::unordered_map` in perf-critical paths | `absl::flat_hash_map` | Cache-unfriendly chaining |
| `std::regex` | RE2, PCRE2, or `ctre` | Up to 100× slower than alternatives |
| `std::endl` | `'\n'` or `std::print` | `endl` flushes the buffer every call |
| `rand()` / `srand()` | `<random>` with `std::mt19937` | Poor statistical properties |
| `new` / `delete` directly | Smart pointers | Manual memory management error-prone |
| `push_back(T(...))` | `emplace_back(...)` | Avoids extra copy/move |
| `std::map::operator[]` to read | `.find()` or `.at()` | `[]` inserts on miss |
| `to_upper` / `to_lower` | ICU or `std::ranges::transform` | No Unicode awareness |
| `s.substr(0,n) == prefix` | `s.starts_with(prefix)` (C++20) | Cleaner, no allocation |
| `std::sort(v.begin(), v.end())` | `std::ranges::sort(v)` (C++20) | No more `.begin()/.end()` pairs |
| `std::optional<T&>` (doesn't compile) | Raw pointer or `reference_wrapper<T>` | `optional`/`expected` can't hold references |
| `std::function<R(Args)>` in hot paths | Template parameter or `auto` lambda | Type-erased; heap-allocates, virtual-dispatches |
| iostream format (`<<` with manipulators) | `std::format` (C++20) / `std::print` (C++23) | Type-safe, readable, faster |
| Erase-remove idiom | `std::erase` / `std::erase_if` (C++20) | One-line, no raw iterator arithmetic |

### `std::function` Overhead
`std::function<R(Args)>` stores any callable via type erasure. The cost:
- **Heap allocation** for any callable that doesn't fit in its small-buffer
- **Indirect (virtual-like) dispatch** on every call
- **Requires copy-constructibility** — move-only callables (e.g., lambdas capturing
  `unique_ptr`) don't compile with `std::function`

Alternatives:
```cpp
// Prefer: template parameter — zero overhead, inlined by the compiler
template <typename F>
void forEach(const std::vector<int>& v, F callback) { … }

// Or: auto — same benefit, works at local scope
auto process = [](int x) { return x * 2; };

// C++23: std::move_only_function — like std::function but accepts move-only callables
std::move_only_function<void()> f = [p = std::make_unique<int>(42)]() { … };
```
Use `std::function` when you genuinely need **type-erased, runtime-polymorphic,
copyable** storage of a callable (e.g., storing callbacks in a container). Everywhere
else, prefer templates or `auto`.
Prefer `std::format` over `printf`-style formatting and `std::cout` with manipulators:
```cpp
// Old: iostream (verbose, stateful, easy to misconfigure)
std::cout << std::fixed << std::setprecision(2) << value << "\n";

// Old: printf (no type safety, format string not checked at compile time)
printf("Value: %.2f\n", value);

// Modern: std::format (type-safe, readable, compile-time checked in C++23)
auto s = std::format("Value: {:.2f}", value);

// C++23: std::print (direct to stdout, no intermediate string)
std::print("Value: {:.2f}\n", value);
```

---

## Namespaces

- Use **one top-level namespace** per project/library.
- Avoid deeply nested namespaces.
- Prefer anonymous namespaces over `static` for translation-unit-private symbols.
- Never `using namespace` in a header.

### ADL and the Namespace Lookup Cascade
Argument-Dependent Lookup (ADL) means the compiler searches the namespaces of a
function's *argument types* in addition to the current scope. Combined with C++'s
lookup cascade (function scope → current namespace → parent namespace → global), this
creates a silent symbol hijacking risk in nested namespaces:

```cpp
namespace company {
    void log(const std::string& msg);  // (A)

    namespace team {
        void process() {
            log("hello");   // resolves to company::log (A) — correct
        }
    }
}
```

If someone later adds `company::team::log`:
```cpp
namespace company {
    namespace team {
        void log(const std::string& msg);  // (B) — new addition
        void process() {
            log("hello");   // now resolves to (B), NOT (A) — silent hijack!
        }
    }
}
```
The call to `log` changes behavior without touching `process()`. This can introduce
security vulnerabilities — for example, silently switching to a test-only,
insecure implementation. The practical rule: **use only one top-level namespace per
project** and avoid nesting beyond one or two levels.

---

## Error Handling Strategy (Pick One Per Codebase)

| Approach | When appropriate |
|----------|-----------------|
| Exceptions | Application-level code; rich error propagation; not across DLL/ABI boundaries |
| `std::expected<T, E>` (C++23) | Library APIs; explicit error paths; performance-sensitive |
| Error codes / out-params | Legacy C interop; embedded/bare-metal |

Mark functions `noexcept` wherever exceptions cannot propagate (destructors, move
constructors, leaf utility functions). This enables compiler optimizations and
documents intent.

If you throw custom types, derive them from `std::exception` (or a standard subclass
like `std::runtime_error`). `throw 42;` and `throw "oops";` both compile, but neither
is catchable by `catch (const std::exception&)`.

Anything can be thrown in C++ — integers, string literals, arbitrary structs. Never
rely on this. Always throw objects derived from `std::exception`.

Apply `[[nodiscard]]` to any function returning an error code, `std::expected`,
`std::optional`, or a resource handle — making it a compile-time warning if the result
is silently discarded. Consider a `[[nodiscard]]` attribute on the return type itself
so it applies everywhere that type is returned.

---

## Concurrency Basics

C++ doesn't have built-in safety for shared mutable state — a **data race is undefined
behavior**, not just "a bug that gives the wrong answer."

- **`std::mutex` + `std::lock_guard`**: the default pattern for protecting shared
  state. `lock_guard` locks on construction, unlocks on destruction (RAII).
- **`std::scoped_lock`** (C++17): like `lock_guard`, but can lock **multiple** mutexes
  at once using a deadlock-avoidance algorithm. Prefer it over nested `lock_guard`s
  when more than one mutex is involved.
- **`std::atomic<T>`**: for simple counters/flags shared across threads without a
  mutex. Don't use it as a substitute for a mutex when multiple related values must
  update together.
- **`std::jthread`** (C++20): like `std::thread`, but automatically joins on
  destruction (no more forgotten `.join()` causing `std::terminate`) and cooperates
  with `std::stop_token` for cancellation.
- Function-local `static` initialization is **guaranteed thread-safe** since C++11 —
  this is what makes the construct-on-first-use idiom (footgun #17) safe from
  multiple threads without extra locking.

Enable ThreadSanitizer (`-fsanitize=thread`) in CI for any multi-threaded code; data
races frequently don't manifest until production load.

---

## Testing Structure

Executables (`.exe`) cannot easily be tested externally. The idiomatic structure is:

```
MyProject/
├── MyLib/        ← static library with all logic
│   └── CMakeLists.txt
├── MyApp/        ← thin wrapper; links MyLib, produces .exe
└── MyTests/      ← test runner; also links MyLib
    └── CMakeLists.txt
```

Choose one framework and stick to it: **GoogleTest** (most widespread), **Catch2**
(header-friendly), or **doctest** (single-header).

**Avoid these anti-patterns for testing private members:**
- `#define private public` — makes *everything* public in every subsequent header
- Making the test class a `friend` — entangles tests with implementation details
- Changing `private` to `protected` and inheriting in tests — misuses inheritance

The correct approach: **refactor private logic into free functions** that live in an
anonymous namespace in the `.cpp` file, then test those functions via the public
interface. Classes become thin wrappers; free functions hold the testable logic.

---

## Build System Advice

- **CMake** is the de-facto standard for open-source and cross-platform projects.
  Use **Modern CMake** (target-based, `target_link_libraries` / `target_include_directories`
  with `PUBLIC`/`PRIVATE`/`INTERFACE`). Avoid global `include_directories` and
  `add_definitions`.
- Use **vcpkg** or **Conan** for dependency management rather than vendoring or
  manual binary integration.
- Compile with **two compilers** (e.g., Clang + GCC) to catch compiler-specific UB
  and get different warning sets.
- Standard warning flags for new projects:
  - GCC/Clang: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnull-dereference`
  - MSVC: `/W4 /WX`
- Enable sanitizers in debug/CI: `-fsanitize=address,undefined`
- **There is no standard C++ ABI.** When linking a precompiled library, the compiler,
  compiler *version*, target architecture, and key flags (optimization level, exception
  handling model, runtime library — `/MD` vs `/MT` on MSVC) must all match your
  project. A mismatch produces bizarre link errors or runtime memory corruption.
  When in doubt, build the dependency from source with your own toolchain.
- Consider a **precompiled header** (PCH) for headers included almost everywhere
  (`<vector>`, `<string>`, project-wide common headers) to cut compile times.

---

## Performance Checklist

Before attributing slowness to the algorithm, check:

1. **Profiler first** — don't guess. Use `perf`, VTune, or Instruments.
2. **Unnecessary copies** — `auto` deduces by value; use `const auto&` for range-for
   over containers of non-trivial types.
3. **Reserve vector capacity** — `v.reserve(n)` before batch `push_back`.
4. **Prefer `emplace_back` over `push_back`** for non-trivial element types.
5. **`std::unordered_map` cache misses** — switch to flat hash map if profiling shows
   hash-lookup hot.
6. **`std::regex` is notoriously slow** — use an alternative library (RE2, PCRE2,
   `ctre`). Benchmarks show 10–100× speed difference.
7. **`std::endl` flushes** — use `'\n'` for streaming output.
8. **Return Value Optimization (RVO/NRVO)** — trust the compiler; do **not** `std::move`
   return values.
9. **Pass small trivial types by value**, not `const&` — `int`, `float`, pointers, etc.
10. **Move constructors must be `noexcept`** — otherwise container reallocation falls
    back to copying (see footgun #23).
11. **Array of Structures vs Structures of Arrays (AoS vs SoA)** — C++ defaults to
    AoS (`std::vector<Particle>` where `Particle` has x, y, z, mass). For
    SIMD-heavy or cache-sensitive loops, SoA (separate `std::vector<float>` for each
    field) can be 2–4× faster by enabling contiguous SIMD loads and eliminating
    struct padding waste. C++ has no built-in SoA support; achieve it manually or
    with libraries. Profile before refactoring — AoS is simpler and fine for most
    code.

---

## Dos and Don'ts Summary

| DO | DON'T |
|----|-------|
| Initialize all variables | Leave POD members uninitialized |
| Mark single-arg constructors `explicit` | Allow implicit construction by default |
| Use `unique_ptr` as default ownership | Default to raw `new`/`delete` or `shared_ptr` |
| Use `{}` for initialization | Mix `()` and `{}` without understanding the difference |
| `noexcept` on move constructors | Let move constructors potentially throw |
| `[[nodiscard]]` on functions returning errors, resources, or results the caller needs | Silently discard return values |
| Enable sanitizers in CI | Ship debug builds without sanitizers |
| Use `std::to_underlying` for enum casts (C++23) | `static_cast<int>` on enums |
| Prefer `.find()` / `.contains()` on maps | Use `operator[]` to read map values |
| Use `.at()` in debug code for bounds-checked access | Assume `[]` is safe |
| Structure projects as lib + thin exe | Put all logic in `main.cpp` |
| Read the assembly when optimizing hot paths | Trust that it's fast without measuring |
| Compose classes from RAII members (Rule of Zero) | Hand-write all five special member functions unless truly needed |
| Use `enum class` | Use plain `enum` |
| Mark overriding functions `override` | Omit `override` and hope signatures still match |
| Pass polymorphic types by reference/pointer | Pass derived objects by value (slicing) |
| Declare members in dependency order | Rely on initializer-list order for member init |
| Use `[[fallthrough]]` for intentional switch fallthrough | Leave a bare fallthrough with no comment |
| Use `std::scoped_lock` for multiple mutexes | Lock multiple mutexes individually in different orders |
| Use structured bindings `auto& [k, v]` for map iteration | Use `.first`/`.second` on pairs |
| Sort struct members largest-to-smallest for minimal padding | Leave member order arbitrary in performance-sensitive structs |
| Prefer concepts over SFINAE for template constraints (C++20) | Write enable_if chains |
| Use `std::erase` / `std::erase_if` (C++20) | Hand-write the erase-remove idiom |
| Use `std::format` / `std::print` for string formatting (C++20/23) | Mix printf and iostream formatting |
| Use trailing underscore for member variables (`count_`) | Prefix identifiers with underscore |
| Use `std::addressof(obj)` when `operator&` might be overloaded | Assume `&obj` always gives the real address |
| Prefer composition over multiple inheritance | Reach for multiple inheritance and virtual bases by default |
| Use `const auto&` in range-for over non-trivial containers | `for (auto item : vec)` — copies every element |
| Use designated initializers for structs with many fields (C++20) | Rely on positional init for structs with 3+ fields |
