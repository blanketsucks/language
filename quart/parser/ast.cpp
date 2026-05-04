#include <quart/parser/ast.h>

namespace quart {

PathSegment::~PathSegment() = default;

PathSegment::PathSegment(PathSegment&&) noexcept = default;
PathSegment& PathSegment::operator=(PathSegment&&) noexcept = default;

PathSegment::PathSegment(String name, Vector<OwnPtr<ast::TypeExpr>> arguments) : m_name(move(name)), m_arguments(move(arguments)) {}

}