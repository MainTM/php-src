/*
   +----------------------------------------------------------------------+
   | Zend JIT                                                             |
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Dmitry Stogov <dmitry@php.net>                              |
   |          Xinchen Hui <xinchen.h@zend.com>                            |
   +----------------------------------------------------------------------+
*/

#include "Zend/zend_execute.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_vm.h"
#include "Zend/zend_closures.h"
#include "Zend/zend_constants.h"
#include "Zend/zend_API.h"

#include <ZendAccelerator.h>
#include "Optimizer/zend_func_info.h"
#include "zend_jit.h"
#include "zend_jit_internal.h"

#ifdef HAVE_GCC_GLOBAL_REGS
# pragma GCC diagnostic ignored "-Wvolatile-register-var"
# if defined(__x86_64__)
register zend_execute_data* volatile execute_data __asm__("%r14");
register const zend_op* volatile opline __asm__("%r15");
# else
register zend_execute_data* volatile execute_data __asm__("%esi");
register const zend_op* volatile opline __asm__("%edi");
# endif
# pragma GCC diagnostic warning "-Wvolatile-register-var"
#endif

ZEND_OPCODE_HANDLER_RET ZEND_FASTCALL zend_jit_leave_nested_func_helper(uint32_t call_info EXECUTE_DATA_DC)
{
	zend_execute_data *old_execute_data;

	if (UNEXPECTED(call_info & ZEND_CALL_HAS_SYMBOL_TABLE)) {
		zend_clean_and_cache_symbol_table(EX(symbol_table));
	}
	EG(current_execute_data) = EX(prev_execute_data);
	if (UNEXPECTED(call_info & ZEND_CALL_RELEASE_THIS)) {
		zend_object *object = Z_OBJ(execute_data->This);
		if (UNEXPECTED(EG(exception) != NULL) && (call_info & ZEND_CALL_CTOR)) {
			GC_DELREF(object);
			zend_object_store_ctor_failed(object);
		}
		OBJ_RELEASE(object);
	} else if (UNEXPECTED(call_info & ZEND_CALL_CLOSURE)) {
		OBJ_RELEASE(ZEND_CLOSURE_OBJECT(EX(func)));
	}

	zend_vm_stack_free_extra_args_ex(call_info, execute_data);
	old_execute_data = execute_data;
	execute_data = EX(prev_execute_data);
	zend_vm_stack_free_call_frame_ex(call_info, old_execute_data);

	if (UNEXPECTED(EG(exception) != NULL)) {
		const zend_op *old_opline = EX(opline);
		zend_throw_exception_internal(NULL);
		if (old_opline->result_type != IS_UNDEF) {
			zval_ptr_dtor(EX_VAR(old_opline->result.var));
		}
#ifndef HAVE_GCC_GLOBAL_REGS
		return 2; // ZEND_VM_LEAVE
#endif
	} else {
		EX(opline)++;
#ifdef HAVE_GCC_GLOBAL_REGS
		opline = EX(opline);
#else
		return 2; // ZEND_VM_LEAVE
#endif
	}
}

ZEND_OPCODE_HANDLER_RET ZEND_FASTCALL zend_jit_leave_top_func_helper(uint32_t call_info EXECUTE_DATA_DC)
{
	if (UNEXPECTED(call_info & (ZEND_CALL_HAS_SYMBOL_TABLE|ZEND_CALL_FREE_EXTRA_ARGS))) {
		if (UNEXPECTED(call_info & ZEND_CALL_HAS_SYMBOL_TABLE)) {
			zend_clean_and_cache_symbol_table(EX(symbol_table));
		}
		zend_vm_stack_free_extra_args_ex(call_info, execute_data);
	}
	EG(current_execute_data) = EX(prev_execute_data);
	if (UNEXPECTED(call_info & ZEND_CALL_CLOSURE)) {
		OBJ_RELEASE(ZEND_CLOSURE_OBJECT(EX(func)));
	}
	execute_data = EG(current_execute_data);
#ifdef HAVE_GCC_GLOBAL_REGS
	opline = zend_jit_halt_op;
#else
	return -1; // ZEND_VM_RETURN
#endif
}

void ZEND_FASTCALL zend_jit_copy_extra_args_helper(EXECUTE_DATA_D)
{
	zend_op_array *op_array = &EX(func)->op_array;

	if (EXPECTED(!(op_array->fn_flags & ZEND_ACC_CALL_VIA_TRAMPOLINE))) {
		uint32_t first_extra_arg = op_array->num_args;
		uint32_t num_args = EX_NUM_ARGS();
		zval *end, *src, *dst;
		uint32_t type_flags = 0;

		if (EXPECTED((op_array->fn_flags & ZEND_ACC_HAS_TYPE_HINTS) == 0)) {
			/* Skip useless ZEND_RECV and ZEND_RECV_INIT opcodes */
#ifdef HAVE_GCC_GLOBAL_REGS
			opline += first_extra_arg;
#endif
		}

		/* move extra args into separate array after all CV and TMP vars */
		end = EX_VAR_NUM(first_extra_arg - 1);
		src = end + (num_args - first_extra_arg);
		dst = src + (op_array->last_var + op_array->T - first_extra_arg);
		if (EXPECTED(src != dst)) {
			do {
				type_flags |= Z_TYPE_INFO_P(src);
				ZVAL_COPY_VALUE(dst, src);
				ZVAL_UNDEF(src);
				src--;
				dst--;
			} while (src != end);
			if (type_flags & (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) {
				ZEND_ADD_CALL_FLAG(execute_data, ZEND_CALL_FREE_EXTRA_ARGS);
			}
		} else {
			do {
				if (Z_REFCOUNTED_P(src)) {
					ZEND_ADD_CALL_FLAG(execute_data, ZEND_CALL_FREE_EXTRA_ARGS);
					break;
				}
				src--;
			} while (src != end);
		}
	}
}

void ZEND_FASTCALL zend_jit_deprecated_or_abstract_helper(OPLINE_D)
{
	zend_function *fbc = ((zend_execute_data*)(opline))->func;

	if (UNEXPECTED((fbc->common.fn_flags & ZEND_ACC_ABSTRACT) != 0)) {
		zend_throw_error(NULL, "Cannot call abstract method %s::%s()", ZSTR_VAL(fbc->common.scope->name), ZSTR_VAL(fbc->common.function_name));
	} else if (UNEXPECTED((fbc->common.fn_flags & ZEND_ACC_DEPRECATED) != 0)) {
		zend_error(E_DEPRECATED, "Function %s%s%s() is deprecated",
			fbc->common.scope ? ZSTR_VAL(fbc->common.scope->name) : "",
			fbc->common.scope ? "::" : "",
			ZSTR_VAL(fbc->common.function_name));
	}
}

ZEND_OPCODE_HANDLER_RET ZEND_FASTCALL zend_jit_profile_helper(ZEND_OPCODE_HANDLER_ARGS)
{
	zend_op_array *op_array = (zend_op_array*)EX(func);
	zend_vm_opcode_handler_t handler = (zend_vm_opcode_handler_t)ZEND_FUNC_INFO(op_array);
	uintptr_t counter = (uintptr_t)ZEND_COUNTER_INFO(op_array);

	ZEND_COUNTER_INFO(op_array) = (void*)(counter + 1);
	++zend_jit_profile_counter;
	ZEND_OPCODE_TAIL_CALL(handler);
}

static zend_always_inline zend_long _op_array_hash(const zend_op_array *op_array)
{
	uintptr_t x;

	if (op_array->function_name) {
		x = (uintptr_t)op_array >> 3;
	} else {
		x = (uintptr_t)op_array->filename >> 3;
	}
#if SIZEOF_SIZE_T == 4
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
#elif SIZEOF_SIZE_T == 8
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
	x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
	x = x ^ (x >> 31);
#endif
	return x;
}

ZEND_OPCODE_HANDLER_RET ZEND_FASTCALL zend_jit_func_counter_helper(ZEND_OPCODE_HANDLER_ARGS)
{
#ifndef HAVE_GCC_GLOBAL_REGS
	const zend_op *opline = EX(opline);
#endif
	unsigned int n = _op_array_hash(&EX(func)->op_array)  %
		(sizeof(zend_jit_hot_counters) / sizeof(zend_jit_hot_counters[0]));

	zend_jit_hot_counters[n] -= ZEND_JIT_HOT_FUNC_COST;

	if (UNEXPECTED(zend_jit_hot_counters[n] <= 0)) {
		zend_jit_hot_counters[n] = ZEND_JIT_HOT_COUNTER_INIT;
		zend_jit_hot_func(execute_data, opline);
		ZEND_OPCODE_RETURN();
	} else {
		zend_vm_opcode_handler_t *handlers =
			(zend_vm_opcode_handler_t*)ZEND_FUNC_INFO(&EX(func)->op_array);
		zend_vm_opcode_handler_t handler = handlers[opline - EX(func)->op_array.opcodes];
		ZEND_OPCODE_TAIL_CALL(handler);
	}
}

ZEND_OPCODE_HANDLER_RET ZEND_FASTCALL zend_jit_loop_counter_helper(ZEND_OPCODE_HANDLER_ARGS)
{
#ifndef HAVE_GCC_GLOBAL_REGS
	const zend_op *opline = EX(opline);
#endif
	unsigned int n = _op_array_hash(&EX(func)->op_array)  %
		(sizeof(zend_jit_hot_counters) / sizeof(zend_jit_hot_counters[0]));

	zend_jit_hot_counters[n] -= ZEND_JIT_HOT_LOOP_COST;

	if (UNEXPECTED(zend_jit_hot_counters[n] <= 0)) {
		zend_jit_hot_counters[n] = ZEND_JIT_HOT_COUNTER_INIT;
		zend_jit_hot_func(execute_data, opline);
		ZEND_OPCODE_RETURN();
	} else {
		zend_vm_opcode_handler_t *handlers =
			(zend_vm_opcode_handler_t*)ZEND_FUNC_INFO(&EX(func)->op_array);
		zend_vm_opcode_handler_t handler = handlers[opline - EX(func)->op_array.opcodes];
		ZEND_OPCODE_TAIL_CALL(handler);
	}
}

static zend_always_inline int _zend_quick_get_constant(
		const zval *key, uint32_t flags, int check_defined_only)
{
#ifndef HAVE_GCC_GLOBAL_REGS
	zend_execute_data *execute_data = EG(current_execute_data);
	const zend_op *opline = EX(opline);
#endif
	zval *zv;
	const zval *orig_key = key;
	zend_constant *c = NULL;

	zv = zend_hash_find_ex(EG(zend_constants), Z_STR_P(key), 1);
	if (zv) {
		c = (zend_constant*)Z_PTR_P(zv);
	} else {
		key++;
		zv = zend_hash_find_ex(EG(zend_constants), Z_STR_P(key), 1);
		if (zv && (ZEND_CONSTANT_FLAGS((zend_constant*)Z_PTR_P(zv)) & CONST_CS) == 0) {
			c = (zend_constant*)Z_PTR_P(zv);
		} else {
			if ((flags & (IS_CONSTANT_IN_NAMESPACE|IS_CONSTANT_UNQUALIFIED)) == (IS_CONSTANT_IN_NAMESPACE|IS_CONSTANT_UNQUALIFIED)) {
				key++;
				zv = zend_hash_find_ex(EG(zend_constants), Z_STR_P(key), 1);
				if (zv) {
					c = (zend_constant*)Z_PTR_P(zv);
				} else {
				    key++;
					zv = zend_hash_find_ex(EG(zend_constants), Z_STR_P(key), 1);
					if (zv && (ZEND_CONSTANT_FLAGS((zend_constant*)Z_PTR_P(zv)) & CONST_CS) == 0) {
						c = (zend_constant*)Z_PTR_P(zv);
					}
				}
			}
		}
	}

	if (!c) {
		if (!check_defined_only) {
			if ((opline->op1.num & IS_CONSTANT_UNQUALIFIED) != 0) {
				char *actual = (char *)zend_memrchr(Z_STRVAL_P(RT_CONSTANT(opline, opline->op2)), '\\', Z_STRLEN_P(RT_CONSTANT(opline, opline->op2)));
				if (!actual) {
					ZVAL_STR_COPY(EX_VAR(opline->result.var), Z_STR_P(RT_CONSTANT(opline, opline->op2)));
				} else {
					actual++;
					ZVAL_STRINGL(EX_VAR(opline->result.var),
							actual, Z_STRLEN_P(RT_CONSTANT(opline, opline->op2)) - (actual - Z_STRVAL_P(RT_CONSTANT(opline, opline->op2))));
				}
				/* non-qualified constant - allow text substitution */
				zend_error(E_WARNING, "Use of undefined constant %s - assumed '%s' (this will throw an Error in a future version of PHP)",
						Z_STRVAL_P(EX_VAR(opline->result.var)), Z_STRVAL_P(EX_VAR(opline->result.var)));
			} else {
				zend_throw_error(NULL, "Undefined constant '%s'", Z_STRVAL_P(RT_CONSTANT(opline, opline->op2)));
				ZVAL_UNDEF(EX_VAR(opline->result.var));
			}
		}
		return FAILURE;
	}

	if (!check_defined_only) {
		ZVAL_COPY_OR_DUP(EX_VAR(opline->result.var), &c->value);
		if (!(ZEND_CONSTANT_FLAGS(c) & (CONST_CS|CONST_CT_SUBST))) {
			const char *ns_sep;
			size_t shortname_offset;
			size_t shortname_len;
			zend_bool is_deprecated;

			if (flags & IS_CONSTANT_UNQUALIFIED) {
				const zval *access_key;

				if (!(flags & IS_CONSTANT_IN_NAMESPACE)) {
					access_key = orig_key - 1;
				} else {
					if (key < orig_key + 2) {
						goto check_short_name;
					} else {
						access_key = orig_key + 2;
					}
				}
				is_deprecated = !zend_string_equals(c->name, Z_STR_P(access_key));
			} else {
check_short_name:
				ns_sep = zend_memrchr(ZSTR_VAL(c->name), '\\', ZSTR_LEN(c->name));
				ZEND_ASSERT(ns_sep);
				/* Namespaces are always case-insensitive. Only compare shortname. */
				shortname_offset = ns_sep - ZSTR_VAL(c->name) + 1;
				shortname_len = ZSTR_LEN(c->name) - shortname_offset;

				is_deprecated = memcmp(ZSTR_VAL(c->name) + shortname_offset, Z_STRVAL_P(orig_key - 1) + shortname_offset, shortname_len) != 0;
			}

			if (is_deprecated) {
				zend_error(E_DEPRECATED,
					"Case-insensitive constants are deprecated. "
					"The correct casing for this constant is \"%s\"",
					ZSTR_VAL(c->name));
				return SUCCESS;
			}
		}
	}

	CACHE_PTR(opline->extended_value, c);
	return SUCCESS;
}

void ZEND_FASTCALL zend_jit_get_constant(const zval *key, uint32_t flags)
{
	_zend_quick_get_constant(key, flags, 0);
}

int ZEND_FASTCALL zend_jit_check_constant(const zval *key)
{
	return _zend_quick_get_constant(key, 0, 1);
}
