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
/**
 * Translates Yul code from EVM dialect to eWasm dialect.
 */

#include <libyul/backends/wasm/EVMToEWasmTranslator.h>

#include <libyul/backends/wasm/WordSizeTransform.h>
#include <libyul/backends/wasm/WasmDialect.h>
#include <libyul/optimiser/ExpressionSplitter.h>

#include <libyul/AsmParser.h>

#include <liblangutil/ErrorReporter.h>
#include <liblangutil/Scanner.h>

using namespace std;
using namespace dev;
using namespace yul;
using namespace langutil;

namespace
{
static string const polyfill{R"({
function or_bool(a, b, c, d) -> r {
	r := i64.ne(0, i64.or(i64.or(a, b), i64.or(c, d)))
}
function add_carry(x, y, c) -> r, r_c {
	let t := i64.add(x, y)
	r := i64.add(t, c)
	r_c := i64.or(
		i64.lt_u(t, x),
		i64.lt_u(r, t)
	)
}
function add(x1, x2, x3, x4, y1, y2, y3, y4) -> r1, r2, r3, r4 {
	let carry
	r4, carry := add_carry(x4, y4, 0)
	r3, carry := add_carry(x3, y3, carry)
	r2, carry := add_carry(x2, y2, carry)
	r1, carry := add_carry(x1, y1, carry)
}
})"};

Block parsePolyfill()
{
	ErrorList errors;
	ErrorReporter errorReporter(errors);
	shared_ptr<WasmDialect> wasmDialect{make_shared<WasmDialect>()};
	shared_ptr<Scanner> scanner{make_shared<Scanner>(CharStream(polyfill, ""))};
	shared_ptr<Block> block = Parser(errorReporter, wasmDialect).parse(scanner, false);
	yulAssert(errors.empty(), "");
	return move(*block);
}
}
void EVMToEWasmTranslator::run(Dialect const& _evmDialect, Block& _ast)
{
	// TODO take care about name clashes with new builins.
	// TODO probably good to also run the function grouper
	// so that we can use arbitrary variables for function parameters.
	NameDispenser nameDispenser{_evmDialect, _ast};
	ExpressionSplitter{_evmDialect, nameDispenser}(_ast);
	WordSizeTransform::run(_ast, nameDispenser);
	_ast.statements += std::move(parsePolyfill().statements);
	// TODO re-parse?
}

