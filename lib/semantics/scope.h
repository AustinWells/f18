// Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FORTRAN_SEMANTICS_SCOPE_H_
#define FORTRAN_SEMANTICS_SCOPE_H_

#include "attr.h"
#include "symbol.h"
#include "../common/idioms.h"
#include "../parser/parse-tree.h"
#include <list>
#include <map>
#include <string>

namespace Fortran::semantics {

class Scope {
  using mapType = std::map<SourceName, Symbol *>;

public:
  // root of the scope tree; contains intrinsics:
  static Scope systemScope;
  static Scope globalScope;  // contains program-units

  ENUM_CLASS(Kind, System, Global, Module, MainProgram, Subprogram, DerivedType)

  Scope(Scope &parent, Kind kind, Symbol *symbol)
    : parent_{parent}, kind_{kind}, symbol_{symbol} {
    if (symbol) {
      symbol->set_scope(this);
    }
  }

  Scope &parent() {
    CHECK(kind_ != Kind::System);
    return parent_;
  }
  const Scope &parent() const {
    CHECK(kind_ != Kind::System);
    return parent_;
  }
  Kind kind() const { return kind_; }
  Symbol *symbol() { return symbol_; }
  const Symbol *symbol() const { return symbol_; }

  const SourceName &name() const {
    CHECK(symbol_);  // must only be called for Scopes known to have a symbol
    return symbol_->name();
  }

  /// Make a scope nested in this one
  Scope &MakeScope(Kind kind, Symbol *symbol = nullptr);

  using size_type = mapType::size_type;
  using iterator = mapType::iterator;
  using const_iterator = mapType::const_iterator;

  iterator begin() { return symbols_.begin(); }
  iterator end() { return symbols_.end(); }
  const_iterator begin() const { return symbols_.begin(); }
  const_iterator end() const { return symbols_.end(); }
  const_iterator cbegin() const { return symbols_.cbegin(); }
  const_iterator cend() const { return symbols_.cend(); }

  iterator find(const SourceName &name);
  const_iterator find(const SourceName &name) const;
  size_type erase(const SourceName &);

  /// Make a Symbol with unknown details.
  std::pair<iterator, bool> try_emplace(
      const SourceName &name, Attrs attrs = Attrs()) {
    return try_emplace(name, attrs, UnknownDetails());
  }
  /// Make a Symbol with provided details.
  template<typename D>
  std::pair<iterator, bool> try_emplace(const SourceName &name, D &&details) {
    return try_emplace(name, Attrs(), details);
  }
  /// Make a Symbol with attrs and details
  template<typename D>
  std::pair<iterator, bool> try_emplace(
      const SourceName &name, Attrs attrs, D &&details) {
    Symbol &symbol{MakeSymbol(name, attrs, std::move(details))};
    return symbols_.insert(std::make_pair(name, &symbol));
  }

  /// Make a Symbol but don't add it to the scope.
  template<typename D>
  Symbol &MakeSymbol(const SourceName &name, Attrs attrs, D &&details) {
    return allSymbols.Make(*this, name, attrs, std::move(details));
  }

  std::list<Scope> &children() { return children_; }
  const std::list<Scope> &children() const { return children_; }

  DerivedTypeSpec &MakeDerivedTypeSpec(const SourceName &);

  std::unique_ptr<parser::CookedSource> cooked_;
  void set_cookedSource(std::unique_ptr<parser::CookedSource> cooked) {
    cooked_ = std::move(cooked);
  }

private:
  Scope &parent_;
  const Kind kind_;
  Symbol *const symbol_;
  std::list<Scope> children_;
  mapType symbols_;
  std::list<DerivedTypeSpec> derivedTypeSpecs_;

  // Storage for all Symbols. Every Symbol is in allSymbols and every Symbol*
  // or Symbol& points to one in there.
  static Symbols<1024> allSymbols;

  friend std::ostream &operator<<(std::ostream &, const Scope &);
};

}  // namespace Fortran::semantics
#endif  // FORTRAN_SEMANTICS_SCOPE_H_
