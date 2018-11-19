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

#include <string>

namespace dev {

/// Simple generic result that holds a value and an optional error message.
/// Results can be implicitly converted to and created from the type of
/// the value they hold.
///
/// Result<bool> check()
/// {
///		if (false)
///			return Result<bool>(false, "Error message.")
///		return true;
/// }
///
///	A result can also be instantiated using one of the factory methods it provides:
///
/// using BoolResult = Result<bool>;
/// BoolResult check()
/// {
///		if (false)
///			return BoolResult::failure("Error message");
///		return BoolResult::success(true);
/// }
///
template <class Type>
class Result
{	
public:
	using Error = std::string;

	/// @{
	/// @name Factory functions
	/// Factory functions that provide a verbose way to create a result
	static Result<Type> success(Type _value) { return Result(_value); }
	static Result<Type> failure(Error _error) { return Result(_error); }
	/// @}

	Result(Type _value): m_value(std::move(_value)) {}
	Result(Error _error): m_error(std::move(_error)) {}
	Result(Type _value, Error _error): m_value(std::move(_value)), m_error(std::move(_error)) {}

	/// @{
	/// @name Wrapper functions
	/// Wrapper functions that provide implicit conversions to and explicit retrieval of
	/// the value this result holds.
	operator Type const&() const { return m_value; }
	Type& operator*() const { return m_value; }
	Type const& get() const { return m_value; }
	Type& get() { return m_value; }
	/// @}

	/// @returns the error message (can be empty).
	std::string const& error() const { return m_error; }

	/// @{
	/// Members are public in order to support structured bindings (starting with C++17)
	Type m_value;
	std::string m_error = Error();
	/// @}
};

}


