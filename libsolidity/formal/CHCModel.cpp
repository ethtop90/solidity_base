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

#include <libsolidity/formal/CHCModel.h>

#include <libsolidity/ast/TypeProvider.h>
#include <libsolidity/formal/Z3CHCInterface.h>
#include <libsolidity/formal/SymbolicTypes.h>

#include <libdevcore/StringUtils.h>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/optional.hpp>

using namespace std;
using namespace dev;
using namespace langutil;
using namespace dev::solidity;

CHCModel::CHCModel(
	smt::EncodingContext& _context,
	ErrorReporter& _errorReporter
):
	m_interface(make_unique<smt::Z3CHCInterface>()),
	m_errorReporter(_errorReporter),
	m_context(_context)
{
}

void CHCModel::analyze(SourceUnit const& _source)
{
	if (_source.annotation().experimentalFeatures.count(ExperimentalFeature::SMTChecker))
		_source.accept(*this);
}

bool CHCModel::visit(ContractDefinition const& _contract)
{
	for (auto const& contract: _contract.annotation().linearizedBaseContracts)
		for (auto var: contract->stateVariables())
			if (*contract == _contract || var->isVisibleInDerivedContracts())
				m_stateVariables.push_back(var);

	vector<smt::Expression> stateExprs;
	vector<smt::Expression> zero;
	for (auto const& var: m_stateVariables)
	{
		m_stateSorts.push_back(smt::smtSort(*var->type()));
		auto const& symbVar = m_context.variable(*var);
		stateExprs.push_back(symbVar->valueAtIndex(0));
		m_interface->declareVariable(symbVar->nameAtIndex(0), *m_stateSorts.back());
		zero.push_back(0);
	}

	auto boolSort = make_shared<smt::Sort>(smt::Kind::Bool);
	auto functionSort = make_shared<smt::FunctionSort>(
		m_stateSorts,
		boolSort
	);
	m_interfacePredicate = make_shared<smt::SymbolicFunctionVariable>(
		functionSort,
		"interface",
		*m_interface
	);
	m_interface->registerRelation(m_interfacePredicate->currentValue());
	smt::Expression interfaceAppl = (*m_interfacePredicate)(zero);//stateExprs);

	if (FunctionDefinition const* constructor = _contract.constructor())
	{
		string constructorName = "constructor_" + to_string(constructor->id());
		m_predicates[constructor] = make_shared<smt::SymbolicFunctionVariable>(
			functionSort,
			constructorName,
			*m_interface
		);
		m_interface->registerRelation(m_predicates.at(constructor)->currentValue());
		smt::Expression constructorAppl = (*m_predicates.at(constructor))(zero);//stateExprs);
		m_interface->addRule(constructorAppl, constructorName);
		smt::Expression constructorInterface = smt::Expression::implies(
			constructorAppl,
			interfaceAppl
		);
		m_interface->addRule(constructorInterface, constructorName + "_to_interface");
	}
	else
		m_interface->addRule(interfaceAppl, "interface");

	auto errorFunctionSort = make_shared<smt::FunctionSort>(
		vector<smt::SortPointer>(),
		boolSort
	);
	m_errorPredicate = make_shared<smt::SymbolicFunctionVariable>(
		errorFunctionSort,
		"error",
		*m_interface
	);
	m_interface->registerRelation(m_errorPredicate->currentValue());

	/// TODO Initialize state variables goes in constructor block.

	return true;
}

bool CHCModel::visit(FunctionDefinition const& _function)
{
	solAssert(!m_currentFunction, "");
	m_currentFunction = &_function;

	declareSymbols(&_function);

	auto boolSort = make_shared<smt::Sort>(smt::Kind::Bool);
	// TODO include local vars.
	auto functionSort = make_shared<smt::FunctionSort>(
		m_stateSorts,
		boolSort
	);

	string predicateName = "function_" + to_string(_function.id());
	m_predicates[&_function] = make_shared<smt::SymbolicFunctionVariable>(
		functionSort,
		predicateName,
		*m_interface
	);
	m_interface->registerRelation(m_predicates.at(&_function)->currentValue());

	smt::EncodingContext::VariableIndices const& beforeFunction = m_context.indicesBeforeBlock(&_function);
	vector<smt::Expression> stateExprs;
	for (auto const& var: m_stateVariables)
	{
		auto const& symbVar = m_context.variable(*var);
		int index = beforeFunction.at(var);
		stateExprs.push_back(symbVar->valueAtIndex(index));
	}
	smt::Expression interfaceAppl = (*m_interfacePredicate)(stateExprs);
	smt::Expression functionAppl = (*m_predicates[&_function])(stateExprs);

	smt::Expression interfaceFunction = smt::Expression::implies(
		interfaceAppl,
		functionAppl
	);
	m_interface->addRule(interfaceFunction, "interface_to_" + predicateName);

	return true;
}

void CHCModel::endVisit(FunctionDefinition const& _function)
{
	smt::EncodingContext::VariableIndices const& beforeFunction = m_context.indicesBeforeBlock(&_function);
	smt::EncodingContext::VariableIndices const& afterFunction = m_context.indicesAfterBlock(&_function);
	vector<smt::Expression> stateExprsBefore;
	vector<smt::Expression> stateExprsAfter;
	for (auto const& var: m_stateVariables)
	{
		auto const& symbVar = m_context.variable(*var);
		stateExprsBefore.push_back(symbVar->valueAtIndex(beforeFunction.at(var)));
		stateExprsAfter.push_back(symbVar->valueAtIndex(afterFunction.at(var)));
	}
	smt::Expression interfaceAppl = (*m_interfacePredicate)(stateExprsAfter);
	smt::Expression functionAppl = (*m_predicates[&_function])(stateExprsBefore);

	smt::Expression interfaceFunction = smt::Expression::implies(
		functionAppl && m_context.constraintsAtStatement(&_function),
		interfaceAppl
	);
	string predicateName = "function_" + to_string(_function.id());
	m_interface->addRule(interfaceFunction, predicateName + "_to_interface");

	solAssert(m_currentFunction == &_function, "");
	m_currentFunction = nullptr;
}

void CHCModel::endVisit(FunctionCall const& _funCall)
{
	solAssert(_funCall.annotation().kind != FunctionCallKind::Unset, "");

	FunctionType const& funType = dynamic_cast<FunctionType const&>(*_funCall.expression().annotation().type);
	if (funType.kind() == FunctionType::Kind::Assert)
		visitAssert(_funCall);
}

void CHCModel::visitAssert(FunctionCall const& _funCall)
{
	auto const& args = _funCall.arguments();
	solAssert(args.size() == 1, "");
	solAssert(args.front()->annotation().type->category() == Type::Category::Bool, "");

	smt::EncodingContext::VariableIndices const& atAssertion = m_context.indicesAtStatement(&_funCall);
	vector<smt::Expression> stateExprs;
	for (auto const& var: m_stateVariables)
	{
		auto const& symbVar = m_context.variable(*var);
		int index = atAssertion.at(var);
		stateExprs.push_back(symbVar->valueAtIndex(index));
	}

	smt::Expression errorAppl = (*m_errorPredicate)({});
	solAssert(m_currentFunction, "");
	smt::Expression functionAppl = (*m_predicates.at(m_currentFunction))(stateExprs);
	smt::Expression assertNeg = !m_context.expression(*args.front())->valueAtIndex(atAssertion.at(args.front().get()));
	smt::Expression assertionError = smt::Expression::implies(
		functionAppl && m_context.constraintsAtStatement(&_funCall) && assertNeg,
		errorAppl
	);
	string predicateName = "assert_" + to_string(_funCall.id());
	m_interface->addRule(assertionError, predicateName + "_to_error");

	query(errorAppl, _funCall.location(), "CHC Assertion violation");
}

void CHCModel::query(smt::Expression const& _query, langutil::SourceLocation const& _location, std::string _description)
{
	smt::CheckResult result;
	vector<string> values;
	tie(result, values) = m_interface->query(_query);
	switch (result)
	{
	case smt::CheckResult::SATISFIABLE:
	{
		std::ostringstream message;
		message << _description << " happens here";
		m_errorReporter.warning(_location, message.str());
		break;
	}
	case smt::CheckResult::UNSATISFIABLE:
		break;
	case smt::CheckResult::UNKNOWN:
		m_errorReporter.warning(_location, _description + " might happen here.");
		break;
	case smt::CheckResult::CONFLICTING:
		m_errorReporter.warning(_location, "At least two SMT solvers provided conflicting answers. Results might not be sound.");
		break;
	case smt::CheckResult::ERROR:
		m_errorReporter.warning(_location, "Error trying to invoke SMT solver.");
		break;
	}
}

void CHCModel::declareSymbols(ASTNode const* _block)
{
	smt::EncodingContext::VariableIndices const& indicesAfterBlock = m_context.indicesAfterBlock(_block);
	for (auto const& node: indicesAfterBlock)
	{
		shared_ptr<smt::SymbolicVariable> symbVar;
		if (auto varDecl = dynamic_cast<VariableDeclaration const*>(node.first))
			symbVar = m_context.variable(*varDecl);
		else if (auto expr = dynamic_cast<Expression const*>(node.first))
			symbVar = m_context.expression(*expr);
		else
			solAssert(false, "");
		solAssert(symbVar, "");
		smt::SortPointer sort = smt::smtSort(*symbVar->type());
		for (int i = 0; i <= node.second; ++i)
			m_interface->declareVariable(symbVar->nameAtIndex(i), *sort);
	}
}
