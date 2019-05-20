/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <libsolidity/formal/EncodingContext.h>
#include <libsolidity/formal/CHCSolverInterface.h>
#include <libsolidity/formal/SymbolicVariables.h>

#include <libsolidity/ast/ASTVisitor.h>
#include <liblangutil/ErrorReporter.h>

namespace langutil
{
class ErrorReporter;
class SourceLocation;
}

namespace dev
{
namespace solidity
{

class CHCModel: private ASTConstVisitor
{
public:
	CHCModel(smt::EncodingContext& _context, langutil::ErrorReporter& _errorReporters);

	void analyze(SourceUnit const& _sources);

private:
	/// Visitor functions.
	//@{
	bool visit(ContractDefinition const& _node) override;
	bool visit(FunctionDefinition const& _node) override;
	void endVisit(FunctionDefinition const& _node) override;
	void endVisit(FunctionCall const& _node) override;
	//@}

	/// Helpers.
	//@{
	void visitAssert(FunctionCall const& _funCall);
	//@}

	/// Solver related.
	//@{
	void query(smt::Expression const& _query, langutil::SourceLocation const& _location, std::string _description);
	void declareSymbols(ASTNode const* _block);
	//@}

	/// Predicates.
	//@{
	/// Artificial Interface predicate.
	/// Single entry block for all functions.
	std::shared_ptr<smt::SymbolicVariable> m_interfacePredicate;

	/// Artificial Error predicate.
	/// Single error block for all assertions.
	std::shared_ptr<smt::SymbolicVariable> m_errorPredicate;

	/// Maps AST nodes to their predicates.
	std::unordered_map<ASTNode const*, std::shared_ptr<smt::SymbolicVariable>> m_predicates;
	//@}

	/// State variables.
	//@{
	/// State variables sorts.
	/// Used by all predicates.
	std::vector<smt::SortPointer> m_stateSorts;
	/// State variables.
	/// Used to create all predicates.
	std::vector<VariableDeclaration const*> m_stateVariables;
	//@}

	/// Verification targets.
	//@{
	std::vector<smt::Expression> m_verificationTargets;
	//@}

	/// Control-flow.
	//@{
	FunctionDefinition const* m_currentFunction = nullptr;
	//@}

	/// CHC solver.
	std::unique_ptr<smt::CHCSolverInterface> m_interface;

	/// ErrorReporter that comes from CompilerStack.
	langutil::ErrorReporter& m_errorReporter;

	/// Stores the context of the encoding.
	smt::EncodingContext& m_context;
};

}
}
