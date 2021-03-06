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

#include "expression.h"
#include "variable.h"
#include "../common/idioms.h"
#include "../parser/characters.h"
#include "../parser/message.h"
#include <ostream>
#include <string>
#include <type_traits>

using namespace Fortran::parser::literals;

namespace Fortran::evaluate {

// Dumping
template<typename... A>
std::ostream &DumpExprWithType(std::ostream &o, const std::variant<A...> &u) {
  std::visit(
      [&](const auto &x) {
        using Ty = typename std::remove_reference_t<decltype(x)>::Result;
        x.Dump(o << '(' << Ty::Dump() << "::") << ')';
      },
      u);
  return o;
}

template<typename... A>
std::ostream &DumpExpr(std::ostream &o, const std::variant<A...> &u) {
  std::visit([&](const auto &x) { x.Dump(o); }, u);
  return o;
}

template<Category CAT>
std::ostream &Expr<AnyKindType<CAT>>::Dump(std::ostream &o) const {
  return DumpExpr(o, u);
}

template<Category CAT>
std::ostream &CategoryComparison<CAT>::Dump(std::ostream &o) const {
  return DumpExpr(o, u);
}

std::ostream &GenericExpr::Dump(std::ostream &o) const {
  return DumpExpr(o, u);
}

template<typename CRTP, typename RESULT, typename A>
std::ostream &Unary<CRTP, RESULT, A>::Dump(
    std::ostream &o, const char *opr) const {
  return operand().Dump(o << opr) << ')';
}

template<typename CRTP, typename RESULT, typename A, typename B>
std::ostream &Binary<CRTP, RESULT, A, B>::Dump(
    std::ostream &o, const char *opr, const char *before) const {
  return right().Dump(left().Dump(o << before) << opr) << ')';
}

template<int KIND>
std::ostream &IntegerExpr<KIND>::Dump(std::ostream &o) const {
  std::visit(common::visitors{[&](const Scalar &n) { o << n.SignedDecimal(); },
                 [&](const CopyableIndirection<DataRef> &d) { d->Dump(o); },
                 [&](const CopyableIndirection<FunctionRef> &d) { d->Dump(o); },
                 [&](const Parentheses &p) { p.Dump(o, "("); },
                 [&](const Negate &n) { n.Dump(o, "(-"); },
                 [&](const Add &a) { a.Dump(o, "+"); },
                 [&](const Subtract &s) { s.Dump(o, "-"); },
                 [&](const Multiply &m) { m.Dump(o, "*"); },
                 [&](const Divide &d) { d.Dump(o, "/"); },
                 [&](const Power &p) { p.Dump(o, "**"); },
                 [&](const Max &m) { m.Dump(o, ",", "MAX("); },
                 [&](const Min &m) { m.Dump(o, ",", "MIN("); },
                 [&](const auto &convert) {
                   DumpExprWithType(o, convert.operand().u);
                 }},
      u_);
  return o;
}

template<int KIND> std::ostream &RealExpr<KIND>::Dump(std::ostream &o) const {
  std::visit(
      common::visitors{[&](const Scalar &n) { o << n.DumpHexadecimal(); },
          [&](const CopyableIndirection<DataRef> &d) { d->Dump(o); },
          [&](const CopyableIndirection<ComplexPart> &d) { d->Dump(o); },
          [&](const CopyableIndirection<FunctionRef> &d) { d->Dump(o); },
          [&](const Parentheses &p) { p.Dump(o, "("); },
          [&](const Negate &n) { n.Dump(o, "(-"); },
          [&](const Add &a) { a.Dump(o, "+"); },
          [&](const Subtract &s) { s.Dump(o, "-"); },
          [&](const Multiply &m) { m.Dump(o, "*"); },
          [&](const Divide &d) { d.Dump(o, "/"); },
          [&](const Power &p) { p.Dump(o, "**"); },
          [&](const IntPower &p) { p.Dump(o, "**"); },
          [&](const Max &m) { m.Dump(o, ",", "MAX("); },
          [&](const Min &m) { m.Dump(o, ",", "MIN("); },
          [&](const RealPart &z) { z.Dump(o, "REAL("); },
          [&](const AIMAG &p) { p.Dump(o, "AIMAG("); },
          [&](const auto &convert) {
            DumpExprWithType(o, convert.operand().u);
          }},
      u_);
  return o;
}

template<int KIND>
std::ostream &ComplexExpr<KIND>::Dump(std::ostream &o) const {
  std::visit(
      common::visitors{[&](const Scalar &n) { o << n.DumpHexadecimal(); },
          [&](const CopyableIndirection<DataRef> &d) { d->Dump(o); },
          [&](const CopyableIndirection<FunctionRef> &d) { d->Dump(o); },
          [&](const Parentheses &p) { p.Dump(o, "("); },
          [&](const Negate &n) { n.Dump(o, "(-"); },
          [&](const Add &a) { a.Dump(o, "+"); },
          [&](const Subtract &s) { s.Dump(o, "-"); },
          [&](const Multiply &m) { m.Dump(o, "*"); },
          [&](const Divide &d) { d.Dump(o, "/"); },
          [&](const Power &p) { p.Dump(o, "**"); },
          [&](const IntPower &p) { p.Dump(o, "**"); },
          [&](const CMPLX &c) { c.Dump(o, ","); }},
      u_);
  return o;
}

template<int KIND>
std::ostream &CharacterExpr<KIND>::Dump(std::ostream &o) const {
  std::visit(common::visitors{[&](const Scalar &s) {
                                o << parser::QuoteCharacterLiteral(s);
                              },
                 [&](const Concat &concat) { concat.Dump(o, "//"); },
                 [&](const Max &m) { m.Dump(o, ",", "MAX("); },
                 [&](const Min &m) { m.Dump(o, ",", "MIN("); },
                 [&](const auto &ind) { ind->Dump(o); }},
      u_);
  return o;
}

template<typename A> std::ostream &Comparison<A>::Dump(std::ostream &o) const {
  o << '(' << A::Dump() << "::";
  this->left().Dump(o);
  o << '.' << EnumToString(this->opr) << '.';
  return this->right().Dump(o) << ')';
}

template<int KIND>
std::ostream &LogicalExpr<KIND>::Dump(std::ostream &o) const {
  std::visit(common::visitors{[&](const Scalar &tf) {
                                o << (tf.IsTrue() ? ".TRUE." : ".FALSE.");
                              },
                 [&](const CopyableIndirection<DataRef> &d) { d->Dump(o); },
                 [&](const CopyableIndirection<FunctionRef> &d) { d->Dump(o); },
                 [&](const Not &n) { n.Dump(o, "(.NOT."); },
                 [&](const And &a) { a.Dump(o, ".AND."); },
                 [&](const Or &a) { a.Dump(o, ".OR."); },
                 [&](const Eqv &a) { a.Dump(o, ".EQV."); },
                 [&](const Neqv &a) { a.Dump(o, ".NEQV."); },
                 [&](const auto &comparison) { comparison.Dump(o); }},
      u_);
  return o;
}

// LEN()
template<int KIND> SubscriptIntegerExpr CharacterExpr<KIND>::LEN() const {
  return std::visit(
      common::visitors{
          [](const Scalar &c) { return SubscriptIntegerExpr{c.size()}; },
          [](const Concat &c) { return c.left().LEN() + c.right().LEN(); },
          [](const Max &c) {
            return SubscriptIntegerExpr{
                SubscriptIntegerExpr::Max{c.left().LEN(), c.right().LEN()}};
          },
          [](const Min &c) {
            return SubscriptIntegerExpr{
                SubscriptIntegerExpr::Max{c.left().LEN(), c.right().LEN()}};
          },
          [](const CopyableIndirection<DataRef> &dr) { return dr->LEN(); },
          [](const CopyableIndirection<Substring> &ss) { return ss->LEN(); },
          [](const CopyableIndirection<FunctionRef> &fr) {
            return fr->proc().LEN();
          }},
      u_);
}

// Rank
template<typename CRTP, typename RESULT, typename A, typename B>
int Binary<CRTP, RESULT, A, B>::Rank() const {
  int lrank{left_.Rank()};
  if (lrank > 0) {
    return lrank;
  }
  return right_.Rank();
}

// Folding
template<typename CRTP, typename RESULT, typename A>
auto Unary<CRTP, RESULT, A>::Fold(FoldingContext &context)
    -> std::optional<Scalar> {
  if (std::optional<OperandScalarConstant> c{operand_->Fold(context)}) {
    return static_cast<CRTP *>(this)->FoldScalar(context, *c);
  }
  return {};
}

template<typename CRTP, typename RESULT, typename A, typename B>
auto Binary<CRTP, RESULT, A, B>::Fold(FoldingContext &context)
    -> std::optional<Scalar> {
  std::optional<LeftScalar> lc{left_->Fold(context)};
  std::optional<RightScalar> rc{right_->Fold(context)};
  if (lc.has_value() && rc.has_value()) {
    return static_cast<CRTP *>(this)->FoldScalar(context, *lc, *rc);
  }
  return {};
}

template<int KIND>
auto IntegerExpr<KIND>::ConvertInteger::FoldScalar(FoldingContext &context,
    const ScalarConstant<Category::Integer> &c) -> std::optional<Scalar> {
  return std::visit(
      [&](auto &x) -> std::optional<Scalar> {
        auto converted{Scalar::ConvertSigned(x)};
        if (converted.overflow) {
          context.messages.Say("integer conversion overflowed"_en_US);
          return {};
        }
        return {std::move(converted.value)};
      },
      c.u);
}

template<int KIND>
auto IntegerExpr<KIND>::ConvertReal::FoldScalar(FoldingContext &context,
    const ScalarConstant<Category::Real> &c) -> std::optional<Scalar> {
  return std::visit(
      [&](auto &x) -> std::optional<Scalar> {
        auto converted{x.template ToInteger<Scalar>()};
        if (converted.flags.test(RealFlag::Overflow)) {
          context.messages.Say("real->integer conversion overflowed"_en_US);
          return {};
        }
        if (converted.flags.test(RealFlag::InvalidArgument)) {
          context.messages.Say(
              "real->integer conversion: invalid argument"_en_US);
          return {};
        }
        return {std::move(converted.value)};
      },
      c.u);
}

template<int KIND>
auto IntegerExpr<KIND>::Negate::FoldScalar(
    FoldingContext &context, const Scalar &c) -> std::optional<Scalar> {
  auto negated{c.Negate()};
  if (negated.overflow) {
    context.messages.Say("integer negation overflowed"_en_US);
    return {};
  }
  return {std::move(negated.value)};
}

template<int KIND>
auto IntegerExpr<KIND>::Add::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  auto sum{a.AddSigned(b)};
  if (sum.overflow) {
    context.messages.Say("integer addition overflowed"_en_US);
    return {};
  }
  return {std::move(sum.value)};
}

template<int KIND>
auto IntegerExpr<KIND>::Subtract::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  auto diff{a.SubtractSigned(b)};
  if (diff.overflow) {
    context.messages.Say("integer subtraction overflowed"_en_US);
    return {};
  }
  return {std::move(diff.value)};
}

template<int KIND>
auto IntegerExpr<KIND>::Multiply::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  auto product{a.MultiplySigned(b)};
  if (product.SignedMultiplicationOverflowed()) {
    context.messages.Say("integer multiplication overflowed"_en_US);
    return {};
  }
  return {std::move(product.lower)};
}

template<int KIND>
auto IntegerExpr<KIND>::Divide::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  auto qr{a.DivideSigned(b)};
  if (qr.divisionByZero) {
    context.messages.Say("integer division by zero"_en_US);
    return {};
  }
  if (qr.overflow) {
    context.messages.Say("integer division overflowed"_en_US);
    return {};
  }
  return {std::move(qr.quotient)};
}

template<int KIND>
auto IntegerExpr<KIND>::Power::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  typename Scalar::PowerWithErrors power{a.Power(b)};
  if (power.divisionByZero) {
    context.messages.Say("zero to negative power"_en_US);
    return {};
  }
  if (power.overflow) {
    context.messages.Say("integer power overflowed"_en_US);
    return {};
  }
  if (power.zeroToZero) {
    context.messages.Say("integer 0**0"_en_US);
    return {};
  }
  return {std::move(power.power)};
}

template<int KIND>
auto IntegerExpr<KIND>::Max::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  if (a.CompareSigned(b) == Ordering::Greater) {
    return {a};
  }
  return {b};
}

template<int KIND>
auto IntegerExpr<KIND>::Min::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  if (a.CompareSigned(b) == Ordering::Less) {
    return {a};
  }
  return {b};
}

template<int KIND>
auto IntegerExpr<KIND>::Fold(FoldingContext &context) -> std::optional<Scalar> {
  return std::visit(
      [&](auto &x) -> std::optional<Scalar> {
        using Ty = typename std::decay<decltype(x)>::type;
        if constexpr (std::is_same_v<Ty, Scalar>) {
          return {x};
        }
        if constexpr (evaluate::FoldableTrait<Ty>) {
          auto c{x.Fold(context)};
          if (c.has_value()) {
            u_ = *c;
            return c;
          }
        }
        return {};
      },
      u_);
}

static void RealFlagWarnings(
    FoldingContext &context, const RealFlags &flags, const char *operation) {
  if (flags.test(RealFlag::Overflow)) {
    context.messages.Say(
        parser::MessageFormattedText("overflow on %s"_en_US, operation));
  }
  if (flags.test(RealFlag::DivideByZero)) {
    context.messages.Say(parser::MessageFormattedText(
        "division by zero on %s"_en_US, operation));
  }
  if (flags.test(RealFlag::InvalidArgument)) {
    context.messages.Say(parser::MessageFormattedText(
        "invalid argument on %s"_en_US, operation));
  }
  if (flags.test(RealFlag::Underflow)) {
    context.messages.Say(
        parser::MessageFormattedText("underflow on %s"_en_US, operation));
  }
}

template<int KIND>
auto RealExpr<KIND>::ConvertInteger::FoldScalar(FoldingContext &context,
    const ScalarConstant<Category::Integer> &c) -> std::optional<Scalar> {
  return std::visit(
      [&](auto &x) -> std::optional<Scalar> {
        auto converted{Scalar::FromInteger(x)};
        RealFlagWarnings(context, converted.flags, "integer->real conversion");
        return {std::move(converted.value)};
      },
      c.u);
}

template<int KIND>
auto RealExpr<KIND>::ConvertReal::FoldScalar(FoldingContext &context,
    const ScalarConstant<Category::Real> &c) -> std::optional<Scalar> {
  return std::visit(
      [&](auto &x) -> std::optional<Scalar> {
        auto converted{Scalar::Convert(x)};
        RealFlagWarnings(context, converted.flags, "real conversion");
        return {std::move(converted.value)};
      },
      c.u);
}

template<int KIND>
auto RealExpr<KIND>::Negate::FoldScalar(
    FoldingContext &context, const Scalar &c) -> std::optional<Scalar> {
  return {c.Negate()};
}

template<int KIND>
auto RealExpr<KIND>::Add::FoldScalar(FoldingContext &context, const Scalar &a,
    const Scalar &b) -> std::optional<Scalar> {
  auto sum{a.Add(b, context.rounding)};
  RealFlagWarnings(context, sum.flags, "real addition");
  return {std::move(sum.value)};
}

template<int KIND>
auto RealExpr<KIND>::Subtract::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  auto difference{a.Subtract(b, context.rounding)};
  RealFlagWarnings(context, difference.flags, "real subtraction");
  return {std::move(difference.value)};
}

template<int KIND>
auto RealExpr<KIND>::Multiply::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  auto product{a.Multiply(b, context.rounding)};
  RealFlagWarnings(context, product.flags, "real multiplication");
  return {std::move(product.value)};
}

template<int KIND>
auto RealExpr<KIND>::Divide::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  auto quotient{a.Divide(b, context.rounding)};
  RealFlagWarnings(context, quotient.flags, "real division");
  return {std::move(quotient.value)};
}

template<int KIND>
auto RealExpr<KIND>::Power::FoldScalar(FoldingContext &context, const Scalar &a,
    const Scalar &b) -> std::optional<Scalar> {
  return {};  // TODO
}

template<int KIND>
auto RealExpr<KIND>::IntPower::FoldScalar(FoldingContext &context,
    const Scalar &a, const ScalarConstant<Category::Integer> &b)
    -> std::optional<Scalar> {
  return {};  // TODO
}

template<int KIND>
auto RealExpr<KIND>::Max::FoldScalar(FoldingContext &context, const Scalar &a,
    const Scalar &b) -> std::optional<Scalar> {
  if (b.IsNotANumber() || a.Compare(b) == Relation::Less) {
    return {b};
  }
  return {a};
}

template<int KIND>
auto RealExpr<KIND>::Min::FoldScalar(FoldingContext &context, const Scalar &a,
    const Scalar &b) -> std::optional<Scalar> {
  if (b.IsNotANumber() || a.Compare(b) == Relation::Greater) {
    return {b};
  }
  return {a};
}

template<int KIND>
auto RealExpr<KIND>::RealPart::FoldScalar(
    FoldingContext &context, const CplxScalar &z) -> std::optional<Scalar> {
  return {z.REAL()};
}

template<int KIND>
auto RealExpr<KIND>::AIMAG::FoldScalar(
    FoldingContext &context, const CplxScalar &z) -> std::optional<Scalar> {
  return {z.AIMAG()};
}

// TODO: generalize over Expr<A> rather than instantiating same for each
template<int KIND>
auto RealExpr<KIND>::Fold(FoldingContext &context) -> std::optional<Scalar> {
  return std::visit(
      [&](auto &x) -> std::optional<Scalar> {
        using Ty = typename std::decay<decltype(x)>::type;
        if constexpr (std::is_same_v<Ty, Scalar>) {
          return {x};
        }
        if constexpr (evaluate::FoldableTrait<Ty>) {
          auto c{x.Fold(context)};
          if (c.has_value()) {
            u_ = *c;
            return c;
          }
        }
        return {};
      },
      u_);
}

template<int KIND>
auto ComplexExpr<KIND>::Fold(FoldingContext &context) -> std::optional<Scalar> {
  return {};  // TODO
}

template<int KIND>
auto CharacterExpr<KIND>::Fold(FoldingContext &context)
    -> std::optional<Scalar> {
  return {};  // TODO
}

template<typename A>
auto Comparison<A>::FoldScalar(FoldingContext &c,
    const OperandScalarConstant &a, const OperandScalarConstant &b)
    -> std::optional<Scalar> {
  if constexpr (A::category == Category::Integer) {
    switch (a.CompareSigned(b)) {
    case Ordering::Less:
      return {opr == RelationalOperator::LE || opr == RelationalOperator::LE ||
          opr == RelationalOperator::NE};
    case Ordering::Equal:
      return {opr == RelationalOperator::LE || opr == RelationalOperator::EQ ||
          opr == RelationalOperator::GE};
    case Ordering::Greater:
      return {opr == RelationalOperator::NE || opr == RelationalOperator::GE ||
          opr == RelationalOperator::GT};
    }
  }
  if constexpr (A::category == Category::Real) {
    switch (a.Compare(b)) {
    case Relation::Less:
      return {opr == RelationalOperator::LE || opr == RelationalOperator::LE ||
          opr == RelationalOperator::NE};
    case Relation::Equal:
      return {opr == RelationalOperator::LE || opr == RelationalOperator::EQ ||
          opr == RelationalOperator::GE};
    case Relation::Greater:
      return {opr == RelationalOperator::NE || opr == RelationalOperator::GE ||
          opr == RelationalOperator::GT};
    case Relation::Unordered: return {};
    }
  }
  // TODO complex and character comparisons
  return {};
}

template<int KIND>
auto LogicalExpr<KIND>::Not::FoldScalar(
    FoldingContext &context, const Scalar &x) -> std::optional<Scalar> {
  return {Scalar{!x.IsTrue()}};
}

template<int KIND>
auto LogicalExpr<KIND>::And::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  return {Scalar{a.IsTrue() && b.IsTrue()}};
}

template<int KIND>
auto LogicalExpr<KIND>::Or::FoldScalar(FoldingContext &context, const Scalar &a,
    const Scalar &b) -> std::optional<Scalar> {
  return {Scalar{a.IsTrue() || b.IsTrue()}};
}

template<int KIND>
auto LogicalExpr<KIND>::Eqv::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  return {Scalar{a.IsTrue() == b.IsTrue()}};
}

template<int KIND>
auto LogicalExpr<KIND>::Neqv::FoldScalar(FoldingContext &context,
    const Scalar &a, const Scalar &b) -> std::optional<Scalar> {
  return {Scalar{a.IsTrue() != b.IsTrue()}};
}

template<int KIND>
auto LogicalExpr<KIND>::Fold(FoldingContext &context) -> std::optional<Scalar> {
  return std::visit(
      [&](auto &x) -> std::optional<Scalar> {
        using Ty = typename std::decay<decltype(x)>::type;
        if constexpr (std::is_same_v<Ty, Scalar>) {
          return {x};
        }
        if constexpr (evaluate::FoldableTrait<Ty>) {
          std::optional<Scalar> c{x.Fold(context)};
          if (c.has_value()) {
            u_ = *c;
            return c;
          }
        }
        return {};
      },
      u_);
}

std::optional<GenericScalar> GenericExpr::ScalarValue() const {
  return std::visit(
      [](const auto &x) -> std::optional<GenericScalar> {
        if (auto c{x.ScalarValue()}) {
          return {GenericScalar{std::move(*c)}};
        }
        return {};
      },
      u);
}

template<Category CAT>
auto Expr<AnyKindType<CAT>>::ScalarValue() const -> std::optional<Scalar> {
  return std::visit(
      [](const auto &x) -> std::optional<Scalar> {
        if (auto c{x.ScalarValue()}) {
          return {Scalar{std::move(*c)}};
        }
        std::optional<Scalar> avoidBogusGCCWarning;  // ... with return {};
        return avoidBogusGCCWarning;
        ;
      },
      u);
}

template<Category CAT>
auto Expr<AnyKindType<CAT>>::Fold(FoldingContext &context)
    -> std::optional<Scalar> {
  return std::visit(
      [&](auto &x) -> std::optional<Scalar> {
        if (auto c{x.Fold(context)}) {
          return {Scalar{std::move(*c)}};
        }
        std::optional<Scalar> avoidBogusGCCWarning;  // ... with return {};
        return avoidBogusGCCWarning;
        ;
      },
      u);
}

std::optional<GenericScalar> GenericExpr::Fold(FoldingContext &context) {
  return std::visit(
      [&](auto &x) -> std::optional<GenericScalar> {
        if (auto c{x.Fold(context)}) {
          return {GenericScalar{std::move(*c)}};
        }
        return {};
      },
      u);
}

template class Expr<AnyKindType<Category::Integer>>;
template class Expr<AnyKindType<Category::Real>>;
template class Expr<AnyKindType<Category::Complex>>;
template class Expr<AnyKindType<Category::Character>>;
template class Expr<AnyKindType<Category::Logical>>;

template class Expr<Type<Category::Integer, 1>>;
template class Expr<Type<Category::Integer, 2>>;
template class Expr<Type<Category::Integer, 4>>;
template class Expr<Type<Category::Integer, 8>>;
template class Expr<Type<Category::Integer, 16>>;
template class Expr<Type<Category::Real, 2>>;
template class Expr<Type<Category::Real, 4>>;
template class Expr<Type<Category::Real, 8>>;
template class Expr<Type<Category::Real, 10>>;
template class Expr<Type<Category::Real, 16>>;
template class Expr<Type<Category::Complex, 2>>;
template class Expr<Type<Category::Complex, 4>>;
template class Expr<Type<Category::Complex, 8>>;
template class Expr<Type<Category::Complex, 10>>;
template class Expr<Type<Category::Complex, 16>>;
template class Expr<Type<Category::Character, 1>>;
template class Expr<Type<Category::Logical, 1>>;
template class Expr<Type<Category::Logical, 2>>;
template class Expr<Type<Category::Logical, 4>>;
template class Expr<Type<Category::Logical, 8>>;

template struct Comparison<Type<Category::Integer, 1>>;
template struct Comparison<Type<Category::Integer, 2>>;
template struct Comparison<Type<Category::Integer, 4>>;
template struct Comparison<Type<Category::Integer, 8>>;
template struct Comparison<Type<Category::Integer, 16>>;
template struct Comparison<Type<Category::Real, 2>>;
template struct Comparison<Type<Category::Real, 4>>;
template struct Comparison<Type<Category::Real, 8>>;
template struct Comparison<Type<Category::Real, 10>>;
template struct Comparison<Type<Category::Real, 16>>;
template struct Comparison<Type<Category::Complex, 2>>;
template struct Comparison<Type<Category::Complex, 4>>;
template struct Comparison<Type<Category::Complex, 8>>;
template struct Comparison<Type<Category::Complex, 10>>;
template struct Comparison<Type<Category::Complex, 16>>;
template struct Comparison<Type<Category::Character, 1>>;
}  // namespace Fortran::evaluate
