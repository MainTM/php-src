/*
   +----------------------------------------------------------------------+
   | Zend OPcache JIT                                                     |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2014 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Dmitry Stogov <dmitry@zend.com>                             |
   +----------------------------------------------------------------------+
*/

/* $Id:$ */

#include <ZendAccelerator.h>

#include "jit/zend_jit_config.h"
#include "jit/zend_jit_context.h"
#include "jit/zend_worklist.h"

void zend_jit_dump_const(zval *zv)
{
	switch (Z_TYPE_P(zv)) {
		case IS_NULL:
			fprintf(stderr, " null");
			break;
		case IS_FALSE:
			fprintf(stderr, " bool(false)");
			break;
		case IS_TRUE:
			fprintf(stderr, " bool(true)");
			break;
		case IS_LONG:
			fprintf(stderr, " int(" ZEND_LONG_FMT ")", Z_LVAL_P(zv));
			break;
		case IS_DOUBLE:
			fprintf(stderr, " float(%g)", Z_DVAL_P(zv));
			break;
		case IS_STRING:
			fprintf(stderr, " string(\"%s\")", Z_STRVAL_P(zv));
			break;
		case IS_ARRAY:
//???		case IS_CONSTANT_ARRAY:
			fprintf(stderr, " array(...)");
			break;
		default:
			fprintf(stderr, " ???");
			break;
	}
}

void zend_jit_dump_var(zend_op_array *op_array, int var_num)
{
	if (var_num < op_array->last_var) {
		fprintf(stderr, "$%s", op_array->vars[var_num]->val);
	} else {
		fprintf(stderr, "$%d", var_num - op_array->last_var);
	}
}

static void zend_jit_dump_info(uint32_t info, zend_class_entry *ce, int is_instanceof)
{
	int first = 1;

	fprintf(stderr, " [");
	if (info & MAY_BE_UNDEF) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "undef");
	}
	if (info & MAY_BE_DEF) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "def");
	}
	if (info & MAY_BE_REF) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "ref");
	}
	if (info & MAY_BE_RC1) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "rc1");
	}
	if (info & MAY_BE_RCN) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "rcn");
	}
	if (info & MAY_BE_CLASS) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "class");
		if (ce) {
			if (is_instanceof) {
				fprintf(stderr, " (instanceof %s)", ce->name->val);
			} else {
				fprintf(stderr, " (%s)", ce->name->val);
			}
		}
	} else if ((info & MAY_BE_ANY) == MAY_BE_ANY) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "any");
	} else {
		if (info & MAY_BE_NULL) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "null");
		}
		if (info & MAY_BE_FALSE) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "false");
		}
		if (info & MAY_BE_TRUE) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "true");
		}
		if (info & MAY_BE_LONG) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "long");
		}
		if (info & MAY_BE_DOUBLE) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "double");
		}
		if (info & MAY_BE_STRING) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "string");
		}
		if (info & MAY_BE_ARRAY) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "array");
			if ((info & MAY_BE_ARRAY_KEY_ANY) != 0 &&
			    (info & MAY_BE_ARRAY_KEY_ANY) != MAY_BE_ARRAY_KEY_ANY) {
				int afirst = 1;
				fprintf(stderr, " [");
				if (info & MAY_BE_ARRAY_KEY_LONG) {
					if (afirst) afirst = 0; else fprintf(stderr, ", ");
					fprintf(stderr, "long");
				}
				if (info & MAY_BE_ARRAY_KEY_STRING) {
					if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "string");
					}
				fprintf(stderr, "]");
			}
			if (info & (MAY_BE_ARRAY_OF_ANY|MAY_BE_ARRAY_OF_REF)) {
				int afirst = 1;
				fprintf(stderr, " of [");
				if ((info & MAY_BE_ARRAY_OF_ANY) == MAY_BE_ARRAY_OF_ANY) {
					if (afirst) afirst = 0; else fprintf(stderr, ", ");
					fprintf(stderr, "any");
				} else {
					if (info & MAY_BE_ARRAY_OF_NULL) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "null");
					}
					if (info & MAY_BE_ARRAY_OF_FALSE) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "false");
					}
					if (info & MAY_BE_ARRAY_OF_TRUE) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "true");
					}
					if (info & MAY_BE_ARRAY_OF_LONG) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "long");
					}
					if (info & MAY_BE_ARRAY_OF_DOUBLE) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "double");
					}
					if (info & MAY_BE_ARRAY_OF_STRING) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "string");
					}
					if (info & MAY_BE_ARRAY_OF_ARRAY) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "array");
					}
					if (info & MAY_BE_ARRAY_OF_OBJECT) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "object");
					}
					if (info & MAY_BE_ARRAY_OF_RESOURCE) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "resource");
					}
				}
				if (info & MAY_BE_ARRAY_OF_REF) {
					if (afirst) afirst = 0; else fprintf(stderr, ", ");
					fprintf(stderr, "ref");
				}
				fprintf(stderr, "]");
			}
		}
		if (info & MAY_BE_OBJECT) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "object");
			if (ce) {
				if (is_instanceof) {
					fprintf(stderr, " (instanceof %s)", ce->name->val);
				} else {
					fprintf(stderr, " (%s)", ce->name->val);
				}
			}
		}
		if (info & MAY_BE_RESOURCE) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "resource");
		}
	}
	if (info & MAY_BE_ERROR) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "error");
	}
	if (info & MAY_BE_IN_REG) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "reg");
	}
	if (info & MAY_BE_REG_ZVAL) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "reg_zval");
	}
	if (info & MAY_BE_REG_ZVAL_PTR) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "reg_zval_ptr");
	}
	if (info & MAY_BE_IN_MEM) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "mem");
	}
	fprintf(stderr, "]");
}

static void zend_jit_dump_ssa_var_info(zend_jit_func_info *func_info, int ssa_var_num)
{
	uint32_t info = get_ssa_var_info(func_info, ssa_var_num);
	zend_class_entry *ce = NULL;
	int is_instanceof = 0;

	if (ssa_var_num >= 0 &&
	    func_info->ssa_var_info &&
	    func_info->ssa_var_info[ssa_var_num].ce) {
		ce = func_info->ssa_var_info[ssa_var_num].ce;
		is_instanceof = func_info->ssa_var_info[ssa_var_num].is_instanceof;
	}
	zend_jit_dump_info(info, ce, is_instanceof);
}

static void zend_jit_dump_range(zend_jit_range *r)
{
	if (r->underflow && r->overflow) {
		return;
	}
	fprintf(stderr, " RANGE[");
	if (r->underflow) {
		fprintf(stderr, "--..");
	} else {
		fprintf(stderr, ZEND_LONG_FMT "..", r->min);
	}
	if (r->overflow) {
		fprintf(stderr, "++]");
	} else {
		fprintf(stderr, ZEND_LONG_FMT "]", r->max);
	}
}

static void zend_jit_dump_pi_range(zend_op_array *op_array, zend_jit_pi_range *r)
{
	zend_jit_func_info *info = JIT_DATA(op_array);

	if (r->range.underflow && r->range.overflow) {
		return;
	}
	fprintf(stderr, " RANGE");
	if (r->negative) {
		fprintf(stderr, "~");
	}
	fprintf(stderr, "[");
	if (r->range.underflow) {
		fprintf(stderr, "-- .. ");
	} else {
		if (r->min_ssa_var >= 0) {
			fprintf(stderr, "#%d(", r->min_ssa_var);
			zend_jit_dump_var(op_array, r->min_var);
			fprintf(stderr, ")");
			if (info->ssa_var_info && info->ssa_var_info[r->min_ssa_var].has_range) {
				zend_jit_dump_range(&info->ssa_var_info[r->min_ssa_var].range);
			}
			if (r->range.min > 0) {
				fprintf(stderr, " + " ZEND_LONG_FMT, r->range.min);
			} else if (r->range.min < 0) {
				fprintf(stderr, " - " ZEND_LONG_FMT, -r->range.min);
			}
			fprintf(stderr, " .. ");
		} else {
			fprintf(stderr, ZEND_LONG_FMT " .. ", r->range.min);
		}
	}
	if (r->range.overflow) {
		fprintf(stderr, "++]");
	} else {
		if (r->max_ssa_var >= 0) {
			fprintf(stderr, "#%d(", r->max_ssa_var);
			zend_jit_dump_var(op_array, r->max_var);
			fprintf(stderr, ")");
			if (info->ssa_var_info && info->ssa_var_info[r->max_ssa_var].has_range) {
				zend_jit_dump_range(&info->ssa_var_info[r->max_ssa_var].range);
			}
			if (r->range.max > 0) {
				fprintf(stderr, " + " ZEND_LONG_FMT, r->range.max);
			} else if (r->range.max < 0) {
				fprintf(stderr, " - " ZEND_LONG_FMT, -r->range.max);
			}
			fprintf(stderr, "]");
		} else {
			fprintf(stderr, ZEND_LONG_FMT "]", r->range.max);
		}
	}
}

void zend_jit_dump_ssa_var(zend_op_array *op_array, int ssa_var_num, int var_num, int pos)
{
	(void) pos;
	zend_jit_func_info *info = JIT_DATA(op_array);

	if (ssa_var_num >= 0) {
		fprintf(stderr, "#%d(", ssa_var_num);
	} else {
		fprintf(stderr, "#?(");
	}
	zend_jit_dump_var(op_array, var_num);
	fprintf(stderr, ")");

	if (info->ssa.vars) {
		if (ssa_var_num >= 0 && info->ssa.vars[ssa_var_num].no_val) {
			fprintf(stderr, " NOVAL");
		}
		zend_jit_dump_ssa_var_info(info, ssa_var_num);
		if (ssa_var_num >= 0 && info->ssa_var_info[ssa_var_num].has_range) {
			zend_jit_dump_range(&info->ssa_var_info[ssa_var_num].range);
		}
	}
}

static void zend_jit_dump_var_set(
	zend_op_array *op_array,
	const char *name,
	zend_bitset set)
{
	int first = 1;
	uint32_t i;

	fprintf(stderr, "    ; %s = {", name);
	for (i = 0; i < op_array->last_var + op_array->T; i++) {
		if (zend_bitset_in(set, i)) {
			if (first) {
				first = 0;
			} else {
				fprintf(stderr, ", ");
			}
			zend_jit_dump_var(op_array, i);
		}
	}
	fprintf(stderr, "}\n");
}

static void zend_jit_dump_block_info(
	zend_op_array *op_array,
	zend_basic_block *block,
	int n)
{
	(void) op_array;

	fprintf(stderr, "  BB%d:\n", n);
	fprintf(stderr, "    ; lines=[%d-%d]\n", block[n].start, block[n].end);
	if ((block[n].flags & ZEND_BB_REACHABLE) == 0) {
		fprintf(stderr, "    ; unreachable\n");
	}
	if (block[n].flags & ZEND_BB_TARGET) {
		fprintf(stderr, "    ; jump target\n");
	}
	if (block[n].flags & ZEND_BB_FOLLOW) {
		fprintf(stderr, "    ; fallthrough control flow\n");
	}
	if (block[n].flags & ZEND_BB_ENTRY) {
		fprintf(stderr, "    ; entry block\n");
	}
	if (block[n].flags & ZEND_BB_TRY) {
		fprintf(stderr, "    ; try\n");
	}
	if (block[n].flags & ZEND_BB_CATCH) {
		fprintf(stderr, "    ; catch\n");
	}
	if (block[n].flags & ZEND_BB_FINALLY) {
		fprintf(stderr, "    ; fnally\n");
	}
	if (block[n].flags & ZEND_BB_GEN_VAR) {
		fprintf(stderr, "    ; gen var\n");
	}
	if (block[n].flags & ZEND_BB_KILL_VAR) {
		fprintf(stderr, "    ; kill var\n");
	}
	if (block[n].flags & ZEND_BB_LOOP_HEADER) {
		fprintf(stderr, "    ; loop header\n");
	}
	if (block[n].flags & ZEND_BB_IRREDUCIBLE_LOOP) {
		fprintf(stderr, "    ; entry to irreducible loop\n");
	}
	if (block[n].loop_header >= 0) {
		fprintf(stderr, "    ; part of loop from block %d\n", block[n].loop_header);
	}
	if (block[n].successors[0] >= 0 || block[n].successors[1] >= 0) {
		fprintf(stderr, "    ; successors={");
		if (block[n].successors[0] >= 0) {
			fprintf(stderr, "%d", block[n].successors[0]);
		}
		if (block[n].successors[1] >= 0) {
			fprintf(stderr, ", %d", block[n].successors[1]);
		}
		fprintf(stderr, "}\n");
	}
	if (block[n].predecessors_count) {
		zend_jit_func_info *info = JIT_DATA(op_array);
		int *predecessors = info->cfg.predecessors;
		int j;

		fprintf(stderr, "    ; predecessors={");
		for (j = 0; j < block[n].predecessors_count; j++) {
			fprintf(stderr, j ? ", %d" : "%d", predecessors[block[n].predecessor_offset + j]);
		}
		fprintf(stderr, "}\n");
	}
	if (block[n].idom >= 0) {
		fprintf(stderr, "    ; idom=%d\n", block[n].idom);
	}
	if (block[n].level >= 0) {
		fprintf(stderr, "    ; level=%d\n", block[n].level);
	}
	if (block[n].children >= 0) {
		int j = block[n].children;
		fprintf(stderr, "    ; children={%d", j);
		j = block[j].next_child;
		while (j >= 0) {
			fprintf(stderr, ", %d", j);
			j = block[j].next_child;
		}
		fprintf(stderr, "}\n");
	}
}

void zend_jit_dump_ssa_bb_header(zend_op_array *op_array, uint32_t line)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_basic_block *blocks = info->cfg.blocks;
	zend_jit_ssa_block *ssa_blocks = info->ssa.blocks;
	int blocks_count = info->cfg.blocks_count;
	int n;

	if (!blocks) return;

	for (n = 0; n < blocks_count; n++) {
		if (line == blocks[n].start) break;
	}
	if (n < blocks_count && line == blocks[n].start) {
		zend_jit_dump_block_info(op_array, blocks, n);
		if (ssa_blocks && ssa_blocks[n].phis) {
			zend_jit_ssa_phi *p = ssa_blocks[n].phis;

			do {
				int j;

				fprintf(stderr, "    ");
				zend_jit_dump_ssa_var(op_array, p->ssa_var, p->var, blocks[n].start);
				if (p->pi < 0) {
					fprintf(stderr, " = Phi(");
					for (j = 0; j < blocks[n].predecessors_count; j++) {
						if (j > 0) {
							fprintf(stderr, ", ");
						}
						zend_jit_dump_ssa_var(op_array, p->sources[j], p->var, blocks[n].start);
					}
					fprintf(stderr, ")\n");
				} else {
					fprintf(stderr, " = Pi(");
					zend_jit_dump_ssa_var(op_array, p->sources[0], p->var, blocks[n].start);
					fprintf(stderr, " &");
					zend_jit_dump_pi_range(op_array, &p->constraint);
					fprintf(stderr, ")\n");
				}
				p = p->next;
			} while (p);
		}
	}
}

void zend_jit_dump_ssa_line(zend_op_array *op_array, const zend_basic_block *b, uint32_t line)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_jit_ssa_op *ssa_ops = info->ssa.ops;
	zend_op *opline = op_array->opcodes + line;
	const char *name = zend_get_opcode_name(opline->opcode);
	uint32_t flags = zend_get_opcode_flags(opline->opcode);
	int n = 0;

	fprintf(stderr, "    ");
	if (ssa_ops) {
		if (ssa_ops[line].result_def >= 0) {
			if (ssa_ops[line].result_use >= 0) {
				zend_jit_dump_ssa_var(op_array, ssa_ops[line].result_use, EX_VAR_TO_NUM(opline->result.var), line);
				fprintf(stderr, " -> ");
			}
			zend_jit_dump_ssa_var(op_array, ssa_ops[line].result_def, EX_VAR_TO_NUM(opline->result.var), line);
			fprintf(stderr, " = ");
		}
	} else if (opline->result_type == IS_CV || opline->result_type == IS_VAR || opline->result_type == IS_TMP_VAR) {
		zend_jit_dump_var(op_array, EX_VAR_TO_NUM(opline->result.var));
		fprintf(stderr, " = ");
	}
	fprintf(stderr, "%s", name ? (name + 5) : "???");

	if (ZEND_VM_OP1_JMP_ADDR & flags) {
		if (b) {
			fprintf(stderr, " BB%d", b->successors[n++]);
		} else {
			fprintf(stderr, " .OP_%u", (uint32_t)(OP_JMP_ADDR(opline, opline->op1) - op_array->opcodes));
		}
	} else if (ZEND_VM_OP1_NUM & flags) {
		fprintf(stderr, " %d", opline->op1.num);
	} else if (opline->op1_type == IS_CONST) {
		zend_jit_dump_const(RT_CONSTANT(op_array, opline->op1));
	} else if (opline->op1_type == IS_CV || opline->op1_type == IS_VAR || opline->op1_type == IS_TMP_VAR) {
	    fprintf(stderr, " ");
	    if (ssa_ops) {
			zend_jit_dump_ssa_var(op_array, ssa_ops[line].op1_use, EX_VAR_TO_NUM(opline->op1.var), line);
			if (ssa_ops[line].op1_def >= 0) {
			    fprintf(stderr, " -> ");
				zend_jit_dump_ssa_var(op_array, ssa_ops[line].op1_def, EX_VAR_TO_NUM(opline->op1.var), line);
			}
		} else {
			zend_jit_dump_var(op_array, EX_VAR_TO_NUM(opline->op1.var));
		}
	}
	if (ZEND_VM_OP2_JMP_ADDR & flags) {
		if (b) {
			fprintf(stderr, " BB%d", b->successors[n]);
		} else {
			fprintf(stderr, " .OP_%u", (uint32_t)(OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes));
		}
	} else if (ZEND_VM_OP2_NUM & flags) {
		fprintf(stderr, " %d", opline->op2.num);
	} else if (opline->op2_type == IS_CONST) {
		zend_jit_dump_const(RT_CONSTANT(op_array, opline->op2));
	} else if (opline->op2_type == IS_CV ||  opline->op2_type == IS_VAR || opline->op2_type == IS_TMP_VAR) {
	    fprintf(stderr, " ");
	    if (ssa_ops) {
			zend_jit_dump_ssa_var(op_array, ssa_ops[line].op2_use, EX_VAR_TO_NUM(opline->op2.var), line);
			if (ssa_ops[line].op2_def >= 0) {
			    fprintf(stderr, " -> ");
				zend_jit_dump_ssa_var(op_array, ssa_ops[line].op2_def, EX_VAR_TO_NUM(opline->op2.var), line);
			}
		} else {
			zend_jit_dump_var(op_array, EX_VAR_TO_NUM(opline->op2.var));
		}
	}
	if (ZEND_VM_EXT_JMP_ADDR & flags) {
		if (opline->opcode != ZEND_CATCH || !opline->result.num) {
			if (b) {
				fprintf(stderr, " BB%d", b->successors[n++]);
			} else {
				fprintf(stderr, " .OP_" ZEND_LONG_FMT, ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value));
			}
		}
	}
	fprintf(stderr, "\n");
}

static void zend_jit_dump_ssa(zend_op_array *op_array)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	int blocks_count = info->cfg.blocks_count;
	int ssa_vars_count = info->ssa.vars_count;
	uint32_t i, j;
	int k;

	if (op_array->function_name) {
		if (op_array->scope && op_array->scope->name) {
			fprintf(stderr, "\n%s::%s", op_array->scope->name->val, op_array->function_name->val);
		} else {
			fprintf(stderr, "\n%s", op_array->function_name->val);
		}
	} else {
		fprintf(stderr, "\n%s", "$_main");
	}
	if (info->clone_num > 0) {
		fprintf(stderr, "__clone_%d", info->clone_num);
	}
	fprintf(stderr, ": ; (lines=%d, args=%d/%d, vars=%d, tmps=%d, blocks=%d, ssa_vars=%d",
		op_array->last,
		op_array->num_args,
		info->num_args,
		op_array->last_var,
		op_array->T,
		blocks_count,
		ssa_vars_count);
	if (info->flags & ZEND_JIT_FUNC_RECURSIVE) {
		fprintf(stderr, ", recursive");
		if (info->flags & ZEND_JIT_FUNC_RECURSIVE_DIRECTLY) {
			fprintf(stderr, " directly");
		}
		if (info->flags & ZEND_JIT_FUNC_RECURSIVE_INDIRECTLY) {
			fprintf(stderr, " indirectly");
		}
	}
	if (info->flags & ZEND_JIT_FUNC_IRREDUCIBLE) {
		fprintf(stderr, ", irreducable");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_LOOPS) {
		fprintf(stderr, ", no_loops");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_IN_MEM_CVS) {
		fprintf(stderr, ", no_in_mem_cvs");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_USED_ARGS) {
		fprintf(stderr, ", no_used_args");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_SYMTAB) {
		fprintf(stderr, ", no_symtab");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_FRAME) {
		fprintf(stderr, ", no_frame");
	}
	if (info->flags & ZEND_JIT_FUNC_INLINE) {
		fprintf(stderr, ", inline");
	}
	if (info->return_value_used == 0) {
		fprintf(stderr, ", no_return_value");
	} else if (info->return_value_used == 1) {
		fprintf(stderr, ", return_value");
	}
	fprintf(stderr, ")\n");

	if (info->num_args > 0) {
		for (i = 0; i < MIN(op_array->num_args, info->num_args ); i++) {
			fprintf(stderr, "    ; arg %d ", i);
			zend_jit_dump_info(info->arg_info[i].info.type, info->arg_info[i].info.ce, info->arg_info[i].info.is_instanceof);
			zend_jit_dump_range(&info->arg_info[i].info.range);
			fprintf(stderr, "\n");
		}
	}
	
	fprintf(stderr, "    ; return ");
	zend_jit_dump_info(info->return_info.type, info->return_info.ce, info->return_info.is_instanceof);
	zend_jit_dump_range(&info->return_info.range);
	fprintf(stderr, "\n");

#if 1
	for (k = 0; k < op_array->last_var; k++) {
		fprintf(stderr, "    ; ");
		if (info->ssa_var_info && (info->ssa_var_info[k].type & (MAY_BE_DEF|MAY_BE_UNDEF|MAY_BE_IN_MEM)) == (MAY_BE_DEF|MAY_BE_IN_MEM)) {
			fprintf(stderr, "preallocated ");
		}
		fprintf(stderr, "CV ");
		zend_jit_dump_ssa_var(op_array, k, k, -1);
		fprintf(stderr, "\n");
	}
#else
	if (info->flags & ZEND_JIT_FUNC_HAS_PREALLOCATED_CVS) {
		for (k = 0; k < op_array->last_var; k++) {
			if ((info->ssa_var_info[k].type & (MAY_BE_DEF|MAY_BE_UNDEF|MAY_BE_IN_MEM)) == (MAY_BE_DEF|MAY_BE_IN_MEM)) {
				fprintf(stderr, "    ; preallocate ");
				zend_jit_dump_ssa_var(op_array, k, k, -1);
				fprintf(stderr, "\n");
			}
		}
	}
#endif
	for (k = 0; k < info->cfg.blocks_count; k++) {
		const zend_basic_block *b = info->cfg.blocks + k;

		zend_jit_dump_ssa_bb_header(op_array,  b->start);
		for (j = b->start; j <= b->end; j++) {
			zend_jit_dump_ssa_line(op_array, b, j);
		}
		/* Insert implicit JMP, introduced by block sorter, if necessary */
		if (b->successors[0] >= 0 &&
		    b->successors[1] < 0 &&
		    b->successors[0] != k + 1) {
			switch (op_array->opcodes[b->end].opcode) {
				case ZEND_JMP:
					break;
				default:
					fprintf(stderr, "    JMP BB%d [implicit]\n", info->cfg.blocks[k].successors[0]);
					break;
			}
		}
	}
}

void zend_jit_dump(zend_op_array *op_array, uint32_t flags)
{
	int j;
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_basic_block *blocks = info->cfg.blocks;
	zend_jit_ssa_block *ssa_blocks = info->ssa.blocks;
	int blocks_count = info->cfg.blocks_count;

	if (flags & JIT_DUMP_CFG) {
		fprintf(stderr, "CFG (lines=%d, blocks=%d)\n", op_array->last, blocks_count);
		for (j = 0; j < blocks_count; j++) {
			zend_jit_dump_block_info(op_array, blocks, j);
		}
	}

	if (flags & JIT_DUMP_DOMINATORS) {
		fprintf(stderr, "Dominators Tree\n");
		for (j = 0; j < blocks_count; j++) {
			zend_jit_dump_block_info(op_array, blocks, j);
		}
	}

	if (flags & JIT_DUMP_PHI_PLACEMENT) {
		fprintf(stderr, "SSA Phi() Placement\n");
		for (j = 0; j < blocks_count; j++) {
			if (ssa_blocks && ssa_blocks[j].phis) {
				zend_jit_ssa_phi *p = ssa_blocks[j].phis;
				int first = 1;

				fprintf(stderr, "  BB%d:\n", j);
				if (p->pi >= 0) {
					fprintf(stderr, "    ; pi={");
				} else {
					fprintf(stderr, "    ; phi={");
				}
				do {
					if (first) {
						first = 0;
					} else {
						fprintf(stderr, ", ");
					}
					zend_jit_dump_var(op_array, p->var);
					p = p->next;
				} while (p);
				fprintf(stderr, "}\n");
			}
		}
	}

	if (flags & JIT_DUMP_VAR) {
		fprintf(stderr, "CV Variables for \"");
		if (op_array->function_name) {
			if (op_array->scope && op_array->scope->name) {
				fprintf(stderr, "%s::%s\":\n", op_array->scope->name->val, op_array->function_name->val);
			} else {
				fprintf(stderr, "%s\":\n", op_array->function_name->val);
			}
		} else {
			fprintf(stderr, "%s\":\n", "$_main");
		}
		for (j = 0; j < op_array->last_var; j++) {
			fprintf(stderr, "  %2d: $%s\n", j, op_array->vars[j]->val);
		}
	}

	if ((flags & JIT_DUMP_VAR_TYPES) && info->ssa.vars) {
		fprintf(stderr, "SSA Variable Types for \"");
		if (op_array->function_name) {
			if (op_array->scope && op_array->scope->name) {
				fprintf(stderr, "%s::%s\":\n", op_array->scope->name->val, op_array->function_name->val);
			} else {
				fprintf(stderr, "%s\":\n", op_array->function_name->val);
			}
		} else {
			fprintf(stderr, "%s\":\n", "$_main");
		}
		for (j = 0; j < info->ssa.vars_count; j++) {
			fprintf(stderr, "  #%d(", j);
			zend_jit_dump_var(op_array, info->ssa.vars[j].var);
			fprintf(stderr, ")");
			if (info->ssa.vars[j].scc >= 0) {
				if (info->ssa.vars[j].scc_entry) {
					fprintf(stderr, " *");
				}  else {
					fprintf(stderr, "  ");
				}
				fprintf(stderr, "SCC=%d", info->ssa.vars[j].scc);
			}
			zend_jit_dump_ssa_var_info(info, j);
			if (info->ssa_var_info && info->ssa_var_info[j].has_range) {
				zend_jit_dump_range(&info->ssa_var_info[j].range);
			}
			fprintf(stderr, "\n");
		}
	}

	if (flags & JIT_DUMP_SSA) {
		zend_jit_dump_ssa(op_array);
	}

}

static void zend_jit_mark_reachable(zend_basic_block *blocks, int n)
{
	while (1) {
		if (blocks[n].flags & ZEND_BB_REACHABLE) return;
		blocks[n].flags |= ZEND_BB_REACHABLE;
		if (blocks[n].successors[0] >= 0) {
			if (blocks[n].successors[1] >= 0) {
				zend_jit_mark_reachable(blocks, blocks[n].successors[0]);
				n = blocks[n].successors[1];
			} else {
				n = blocks[n].successors[0];
			}
		} else {
			return;
		}
	}
}

static void record_successor(zend_basic_block *blocks, int pred, int n, int succ)
{
	blocks[pred].successors[n] = succ;
	blocks[succ].predecessors_count++;
}

#define BB_START(i, flag) do { \
		if (!block_map[i]) { blocks_count++;} \
		block_map[i] |= (flag); \
	} while (0)

int zend_jit_build_cfg(zend_jit_context *ctx, zend_op_array *op_array)
{
	zend_jit_func_info *info = JIT_DATA(op_array);

	if (zend_build_cfg(&ctx->arena, op_array, 1, 0, &info->cfg, &info->flags) != SUCCESS) {
		return FAILURE;
	}

	if (zend_cfg_build_predecessors(&ctx->arena, &info->cfg) != SUCCESS) {
		return FAILURE;
	}

	if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_CFG) {
		zend_jit_dump(op_array, JIT_DUMP_CFG);
	}

	return SUCCESS;
}

static int zend_jit_compute_dominators_tree(zend_op_array *op_array)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_basic_block *blocks = info->cfg.blocks;
	int blocks_count = info->cfg.blocks_count;
	int j, k, changed;

	/* FIXME: move declarations */
	blocks[0].idom = 0;
	do {
		changed = 0;
		for (j = 1; j < blocks_count; j++) {
			int idom = -1;

			if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
				continue;
			}
			for (k = 0; k < blocks[j].predecessors_count; k++) {
				int pred = info->cfg.predecessors[blocks[j].predecessor_offset + k];

				if (idom < 0) {
					if (blocks[pred].idom >= 0)
						idom = pred;
					continue;
				}

				if (blocks[pred].idom >= 0) {
					while (idom != pred) {
						while (pred > idom) pred = blocks[pred].idom;
						while (idom > pred) idom = blocks[idom].idom;
					}
				}
			}

			if (idom >= 0 && blocks[j].idom != idom) {
				blocks[j].idom = idom;
				changed = 1;
			}
		}
	} while (changed);
	blocks[0].idom = -1;

	for (j = 1; j < blocks_count; j++) {
		if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
			continue;
		}
		if (blocks[j].idom >= 0) {
			/* Sort by block number to traverse children in pre-order */
			if (blocks[blocks[j].idom].children < 0 ||
			    j < blocks[blocks[j].idom].children) {
				blocks[j].next_child = blocks[blocks[j].idom].children;
				blocks[blocks[j].idom].children = j;
			} else {
				int k = blocks[blocks[j].idom].children;
				while (blocks[k].next_child >=0 && j > blocks[k].next_child) {
					k = blocks[k].next_child;
				}
				blocks[j].next_child = blocks[k].next_child;
				blocks[k].next_child = j;
			}
		}
	}

	for (j = 0; j < blocks_count; j++) {
		int idom = blocks[j].idom, level = 0;
		if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
			continue;
		}
		while (idom >= 0) {
			level++;
			if (blocks[idom].level >= 0) {
				level += blocks[idom].level;
				break;
			} else {
				idom = blocks[idom].idom;
			}
		}
		blocks[j].level = level;
	}

	if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_DOMINATORS) {
		zend_jit_dump(op_array, JIT_DUMP_DOMINATORS);
	}

	return SUCCESS;
}

static int dominates(zend_op_array *op_array, int a, int b)
{
	zend_jit_func_info *info = JIT_DATA(op_array);

	while (info->cfg.blocks[b].level > info->cfg.blocks[a].level)
		b = info->cfg.blocks[b].idom;
	return a == b;
}

static int zend_jit_identify_loops(zend_op_array *op_array)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	int i, j, k;
	int depth;
	zend_basic_block *blocks = info->cfg.blocks;
	int *dj_spanning_tree;
	zend_worklist work;
	int flag = ZEND_JIT_FUNC_NO_LOOPS;

	ZEND_WORKLIST_ALLOCA(&work, info->cfg.blocks_count);
	dj_spanning_tree = alloca(sizeof(int) * info->cfg.blocks_count);

	for (i = 0; i < info->cfg.blocks_count; i++) {
		dj_spanning_tree[i] = -1;
	}
	zend_worklist_push(&work, 0);
	while (zend_worklist_len(&work)) {
	next:
		i = zend_worklist_peek(&work);
		/* Visit blocks immediately dominated by i. */
		for (j = blocks[i].children; j >= 0; j = blocks[j].next_child)
			if (zend_worklist_push(&work, j)) {
				dj_spanning_tree[j] = i;
				goto next;
			}
		/* Visit join edges.  */
		for (j = 0; j < 2; j++) {
			int succ = blocks[i].successors[j];
			if (succ < 0) {
				continue;
			} else if (blocks[succ].idom == i) {
				continue;
			} else if (zend_worklist_push(&work, succ)) {
				dj_spanning_tree[succ] = i;
				goto next;
			}
		}
		zend_worklist_pop(&work);
	}

	/* Identify loops.  See Sreedhar et al, "Identifying Loops Using DJ
	   Graphs".  */

	for (i = 0, depth = 0; i < info->cfg.blocks_count; i++) {
		if (blocks[i].level > depth)
			depth = blocks[i].level;
	}
	for (; depth >= 0; depth--) {
		for (i = 0; i < info->cfg.blocks_count; i++) {
			if (blocks[i].level != depth) {
				continue;
			}
			zend_bitset_clear(work.visited, zend_bitset_len(info->cfg.blocks_count));
			for (j = 0; j < blocks[i].predecessors_count; j++) {
				int pred = info->cfg.predecessors[blocks[i].predecessor_offset + j];

				/* A join edge is one for which the predecessor does not
				   immediately dominate the successor.  */
				if (blocks[i].idom == pred) {
					continue;
				}

				/* In a loop back-edge (back-join edge), the successor dominates
				   the predecessor.  */
				if (dominates(op_array, i, pred)) {
					blocks[i].flags |= ZEND_BB_LOOP_HEADER;
					flag &= ~ZEND_JIT_FUNC_NO_LOOPS;
					zend_worklist_push(&work, pred);
				} else {
					/* Otherwise it's a cross-join edge.  See if it's a branch
					   to an ancestor on the dominator spanning tree.  */
					int dj_parent = pred;
					while (dj_parent >= 0) {
						if (dj_parent == i) {
							/* An sp-back edge: mark as irreducible.  */
							blocks[i].flags |= ZEND_BB_IRREDUCIBLE_LOOP;
							flag |= ZEND_JIT_FUNC_IRREDUCIBLE;
							flag &= ~ZEND_JIT_FUNC_NO_LOOPS;
							break;
						} else {
							dj_parent = dj_spanning_tree[dj_parent];
						}
					}
				}
			}
			while (zend_worklist_len(&work)) {
				j = zend_worklist_pop(&work);
				if (blocks[j].loop_header < 0 && j != i) {
					blocks[j].loop_header = i;
					for (k = 0; k < blocks[j].predecessors_count; k++) {
						zend_worklist_push(&work, info->cfg.predecessors[blocks[j].predecessor_offset + k]);
					}
				}
			}
		}
	}

	info->flags |= flag;

	return SUCCESS;
}

static int zend_jit_compute_dfg(zend_jit_dfg *dfg, zend_op_array *op_array)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	int set_size;
	zend_basic_block *blocks = info->cfg.blocks;
	int blocks_count = info->cfg.blocks_count;
	zend_bitset tmp, gen, def, use, in, out;
	zend_op *opline;
	uint32_t k;
	int j, changed;

	/* FIXME: can we use "gen" instead of "def" for flow analyzing? */
	set_size = dfg->size;
	tmp = dfg->tmp;
	gen = dfg->gen;
	def = dfg->def;
	use = dfg->use;
	in  = dfg->in;
	out = dfg->out;

	/* Collect "gen", "def" and "use" sets */
	for (j = 0; j < blocks_count; j++) {
		if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
			continue;
		}
		for (k = blocks[j].start; k <= blocks[j].end; k++) {
			opline = op_array->opcodes + k;
			if (opline->opcode != ZEND_OP_DATA) {
				zend_op *next = opline + 1;
				if (k < blocks[j].end &&
					next->opcode == ZEND_OP_DATA) {
					if (next->op1_type & (IS_CV|IS_VAR|IS_TMP_VAR)) {
						if (!zend_bitset_in(def + (j * set_size), EX_VAR_TO_NUM(next->op1.var))) {
							zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(next->op1.var));
						}
					}
					if (next->op2_type == IS_CV) {
						if (!zend_bitset_in(def + (j * set_size), EX_VAR_TO_NUM(next->op2.var))) {
							zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(next->op2.var));
						}
					} else if (next->op2_type == IS_VAR ||
							   next->op2_type == IS_TMP_VAR) {
						/* ZEND_ASSIGN_??? use the second operand
						   of the following OP_DATA instruction as
						   a temporary variable */
						switch (opline->opcode) {
							case ZEND_ASSIGN_DIM:
							case ZEND_ASSIGN_OBJ:
							case ZEND_ASSIGN_ADD:
							case ZEND_ASSIGN_SUB:
							case ZEND_ASSIGN_MUL:
							case ZEND_ASSIGN_DIV:
							case ZEND_ASSIGN_MOD:
							case ZEND_ASSIGN_SL:
							case ZEND_ASSIGN_SR:
							case ZEND_ASSIGN_CONCAT:
							case ZEND_ASSIGN_BW_OR:
							case ZEND_ASSIGN_BW_AND:
							case ZEND_ASSIGN_BW_XOR:
							case ZEND_ASSIGN_POW:
								break;
							default:
								if (!zend_bitset_in(def + (j * set_size), EX_VAR_TO_NUM(next->op2.var))) {
									zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(next->op2.var));
								}
						}
					}
				}
				if (opline->op1_type == IS_CV) {
					switch (opline->opcode) {
					case ZEND_ASSIGN:
					case ZEND_ASSIGN_REF:
					case ZEND_BIND_GLOBAL:
					case ZEND_SEND_VAR_EX:
					case ZEND_SEND_REF:
					case ZEND_SEND_VAR_NO_REF:
					case ZEND_FE_RESET_R:
					case ZEND_FE_RESET_RW:
					case ZEND_ADD_ARRAY_ELEMENT:
					case ZEND_INIT_ARRAY:
						if (!zend_bitset_in(use + (j * set_size), EX_VAR_TO_NUM(opline->op1.var))) {
							// FIXME: include into "use" to ...?
							zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(opline->op1.var));
							zend_bitset_incl(def + (j * set_size), EX_VAR_TO_NUM(opline->op1.var));
						}
						zend_bitset_incl(gen + (j * set_size), EX_VAR_TO_NUM(opline->op1.var));
						break;
					case ZEND_ASSIGN_ADD:
					case ZEND_ASSIGN_SUB:
					case ZEND_ASSIGN_MUL:
					case ZEND_ASSIGN_DIV:
					case ZEND_ASSIGN_MOD:
					case ZEND_ASSIGN_SL:
					case ZEND_ASSIGN_SR:
					case ZEND_ASSIGN_CONCAT:
					case ZEND_ASSIGN_BW_OR:
					case ZEND_ASSIGN_BW_AND:
					case ZEND_ASSIGN_BW_XOR:
					case ZEND_ASSIGN_POW:
					case ZEND_PRE_INC:
					case ZEND_PRE_DEC:
					case ZEND_POST_INC:
					case ZEND_POST_DEC:
					case ZEND_ASSIGN_DIM:
					case ZEND_ASSIGN_OBJ:
					case ZEND_UNSET_DIM:
					case ZEND_UNSET_OBJ:
					case ZEND_FETCH_DIM_W:
					case ZEND_FETCH_DIM_RW:
					case ZEND_FETCH_DIM_FUNC_ARG:
					case ZEND_FETCH_DIM_UNSET:
					case ZEND_FETCH_OBJ_W:
					case ZEND_FETCH_OBJ_RW:
					case ZEND_FETCH_OBJ_FUNC_ARG:
					case ZEND_FETCH_OBJ_UNSET:
						zend_bitset_incl(gen + (j * set_size), EX_VAR_TO_NUM(opline->op1.var));
					default:
						if (!zend_bitset_in(def + (j * set_size), EX_VAR_TO_NUM(opline->op1.var))) {
							zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(opline->op1.var));
						}
					}
				} else if (opline->op1_type == IS_VAR ||
						   opline->op1_type == IS_TMP_VAR) {
					if (!zend_bitset_in(def + (j * set_size), EX_VAR_TO_NUM(opline->op1.var))) {
						zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(opline->op1.var));
					}
				}
				if (opline->op2_type == IS_CV) {
					switch (opline->opcode) {
						case ZEND_ASSIGN:
						case ZEND_ASSIGN_REF:
						case ZEND_FE_FETCH_R:
						case ZEND_FE_FETCH_RW:
							if (!zend_bitset_in(use + (j * set_size), EX_VAR_TO_NUM(opline->op2.var))) {
								// FIXME: include into "use" to ...?
								zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(opline->op2.var));
								zend_bitset_incl(def + (j * set_size), EX_VAR_TO_NUM(opline->op2.var));
							}
							zend_bitset_incl(gen + (j * set_size), EX_VAR_TO_NUM(opline->op2.var));
							break;
						default:
							if (!zend_bitset_in(def + (j * set_size), EX_VAR_TO_NUM(opline->op2.var))) {
								zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(opline->op2.var));
							}
							break;
					}
				} else if (opline->op2_type == IS_VAR ||
						   opline->op2_type == IS_TMP_VAR) {
					if (opline->opcode == ZEND_FE_FETCH_R || opline->opcode == ZEND_FE_FETCH_RW) {
						if (!zend_bitset_in(use + (j * set_size), EX_VAR_TO_NUM(opline->op2.var))) {
							zend_bitset_incl(def + (j * set_size), EX_VAR_TO_NUM(opline->op2.var));
						}
						zend_bitset_incl(gen + (j * set_size), EX_VAR_TO_NUM(opline->op2.var));
					} else {
						if (!zend_bitset_in(def + (j * set_size), EX_VAR_TO_NUM(opline->op2.var))) {
							zend_bitset_incl(use + (j * set_size), EX_VAR_TO_NUM(opline->op2.var));
						}
					}
				}
				if (opline->result_type == IS_CV) {
					if (!zend_bitset_in(use + (j * set_size), EX_VAR_TO_NUM(opline->result.var))) {
						zend_bitset_incl(def + (j * set_size), EX_VAR_TO_NUM(opline->result.var));
					}
					zend_bitset_incl(gen + (j * set_size), EX_VAR_TO_NUM(opline->result.var));
				} else if (opline->result_type == IS_VAR ||
						   opline->result_type == IS_TMP_VAR) {
					if (!zend_bitset_in(use + (j * set_size), EX_VAR_TO_NUM(opline->result.var))) {
						zend_bitset_incl(def + (j * set_size), EX_VAR_TO_NUM(opline->result.var));
					}
					zend_bitset_incl(gen + (j * set_size), EX_VAR_TO_NUM(opline->result.var));
				}
				if ((opline->opcode == ZEND_FE_FETCH_R || opline->opcode == ZEND_FE_FETCH_RW) && opline->result_type == IS_TMP_VAR) {
					if (!zend_bitset_in(use + (j * set_size), EX_VAR_TO_NUM(next->result.var))) {
						zend_bitset_incl(def + (j * set_size), EX_VAR_TO_NUM(next->result.var));
					}
					zend_bitset_incl(gen + (j * set_size), EX_VAR_TO_NUM(next->result.var));
				}
			}
		}
	}

	/* Calculate "in" and "out" sets */
	do {
		changed = 0;
		for (j = 0; j < blocks_count; j++) {
			if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
				continue;
			}
			if (blocks[j].successors[0] >= 0) {
				zend_bitset_copy(out + (j * set_size), in + (blocks[j].successors[0] * set_size), set_size);
				if (blocks[j].successors[1] >= 0) {
					zend_bitset_union(out + (j * set_size), in + (blocks[j].successors[1] * set_size), set_size);
				}
			} else {
				zend_bitset_clear(out + (j * set_size), set_size);
			}
			zend_bitset_union_with_difference(tmp, use + (j * set_size), out + (j * set_size), def + (j * set_size), set_size);
			if (!zend_bitset_equal(in + (j * set_size), tmp, set_size)) {
				zend_bitset_copy(in + (j * set_size), tmp, set_size);
				changed = 1;
			}
		}
	} while (changed);

	if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_LIVENESS) {
		fprintf(stderr, "Variable Liveness\n");
		for (j = 0; j < blocks_count; j++) {
			fprintf(stderr, "  BB%d:\n", j);
			zend_jit_dump_var_set(op_array, "gen", dfg->gen + (j * dfg->size));
			zend_jit_dump_var_set(op_array, "def", dfg->def + (j * dfg->size));
			zend_jit_dump_var_set(op_array, "use", dfg->use + (j * dfg->size));
			zend_jit_dump_var_set(op_array, "in ", dfg->in  + (j * dfg->size));
			zend_jit_dump_var_set(op_array, "out", dfg->out + (j * dfg->size));
		}
	}

	return SUCCESS;
}

static int zend_jit_ssa_rename(zend_op_array *op_array, int *var, int n)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_basic_block *blocks = info->cfg.blocks;
	zend_jit_ssa_block *ssa_blocks = info->ssa.blocks;
	zend_jit_ssa_op *ssa_ops = info->ssa.ops;
	int ssa_vars_count = info->ssa.vars_count;
	int i, j;
	uint32_t k;
	zend_op *opline;
	int *tmp = NULL;

	// FIXME: Can we optimize this copying out in some cases?
	if (blocks[n].next_child >= 0) {
		tmp = alloca(sizeof(int) * (op_array->last_var + op_array->T));
		memcpy(tmp, var, sizeof(int) * (op_array->last_var + op_array->T));
		var = tmp;
	}

	if (ssa_blocks[n].phis) {
		zend_jit_ssa_phi *phi = ssa_blocks[n].phis;
		do {
			if (phi->ssa_var < 0) {
				phi->ssa_var = ssa_vars_count;
				var[phi->var] = ssa_vars_count;
				ssa_vars_count++;
			} else {
				var[phi->var] = phi->ssa_var;
			}
			phi = phi->next;
		} while (phi);
	}

	for (k = blocks[n].start; k <= blocks[n].end; k++) {
		opline = op_array->opcodes + k;
		if (opline->opcode != ZEND_OP_DATA) {
			zend_op *next = opline + 1;
			if (k < blocks[n].end &&
			    next->opcode == ZEND_OP_DATA) {
				if (next->op1_type == IS_CV) {
					ssa_ops[k + 1].op1_use = var[EX_VAR_TO_NUM(next->op1.var)];
					//USE_SSA_VAR(next->op1.var);
				} else if (next->op1_type == IS_VAR ||
				           next->op1_type == IS_TMP_VAR) {
					ssa_ops[k + 1].op1_use = var[EX_VAR_TO_NUM(next->op1.var)];
					//USE_SSA_VAR(op_array->last_var + next->op1.var);
				}
				if (next->op2_type == IS_CV) {
					ssa_ops[k + 1].op2_use = var[EX_VAR_TO_NUM(next->op2.var)];
					//USE_SSA_VAR(next->op2.var);
				} else if (next->op2_type == IS_VAR ||
				           next->op2_type == IS_TMP_VAR) {
					/* ZEND_ASSIGN_??? use the second operand
					   of the following OP_DATA instruction as
					   a temporary variable */
					switch (opline->opcode) {
						case ZEND_ASSIGN_DIM:
						case ZEND_ASSIGN_OBJ:
						case ZEND_ASSIGN_ADD:
						case ZEND_ASSIGN_SUB:
						case ZEND_ASSIGN_MUL:
						case ZEND_ASSIGN_DIV:
						case ZEND_ASSIGN_MOD:
						case ZEND_ASSIGN_SL:
						case ZEND_ASSIGN_SR:
						case ZEND_ASSIGN_CONCAT:
						case ZEND_ASSIGN_BW_OR:
						case ZEND_ASSIGN_BW_AND:
						case ZEND_ASSIGN_BW_XOR:
						case ZEND_ASSIGN_POW:
							break;
						default:
							ssa_ops[k + 1].op2_use = var[EX_VAR_TO_NUM(next->op2.var)];
							//USE_SSA_VAR(op_array->last_var + next->op2.var);
					}
				}
			}
			if (opline->op1_type & (IS_CV|IS_VAR|IS_TMP_VAR)) {
				ssa_ops[k].op1_use = var[EX_VAR_TO_NUM(opline->op1.var)];
				//USE_SSA_VAR(op_array->last_var + opline->op1.var)
			}
			if (opline->opcode == ZEND_FE_FETCH_R || opline->opcode == ZEND_FE_FETCH_RW) {
				if (opline->op2_type == IS_CV) {
					ssa_ops[k].op2_use = var[EX_VAR_TO_NUM(opline->op2.var)];
				}
				ssa_ops[k].op2_def = ssa_vars_count;
				var[EX_VAR_TO_NUM(opline->op2.var)] = ssa_vars_count;
				ssa_vars_count++;
				//NEW_SSA_VAR(opline->op2.var)
			} else if (opline->op2_type & (IS_CV|IS_VAR|IS_TMP_VAR)) {
				ssa_ops[k].op2_use = var[EX_VAR_TO_NUM(opline->op2.var)];
				//USE_SSA_VAR(op_array->last_var + opline->op2.var)
			}
			switch (opline->opcode) {
				case ZEND_ASSIGN:
					if (opline->op1_type == IS_CV) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op1.var)
					}
					if (opline->op2_type == IS_CV) {
						ssa_ops[k].op2_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op2.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op2.var)
					}
					break;
				case ZEND_ASSIGN_REF:
//TODO: ???
					if (opline->op1_type == IS_CV) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op1.var)
					}
					if (opline->op2_type == IS_CV) {
						ssa_ops[k].op2_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op2.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op2.var)
					}
					break;
				case ZEND_BIND_GLOBAL:
					if (opline->op1_type == IS_CV) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op1.var)
					}
					break;
				case ZEND_ASSIGN_DIM:
				case ZEND_ASSIGN_OBJ:
					if (opline->op1_type == IS_CV) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op1.var)
					}
					if (next->op1_type == IS_CV) {
						ssa_ops[k + 1].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(next->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(next->op1.var)
					}
					break;
				case ZEND_ADD_ARRAY_ELEMENT:
					ssa_ops[k].result_use = var[EX_VAR_TO_NUM(opline->result.var)];
				case ZEND_INIT_ARRAY:
					if (opline->op1_type == IS_CV) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline+->op1.var)
					}
					break;
				case ZEND_SEND_VAR_NO_REF:
				case ZEND_SEND_VAR_EX:
				case ZEND_SEND_REF:
				case ZEND_FE_RESET_R:
				case ZEND_FE_RESET_RW:
//TODO: ???
					if (opline->op1_type == IS_CV) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op1.var)
					}
					break;
				case ZEND_ASSIGN_ADD:
				case ZEND_ASSIGN_SUB:
				case ZEND_ASSIGN_MUL:
				case ZEND_ASSIGN_DIV:
				case ZEND_ASSIGN_MOD:
				case ZEND_ASSIGN_SL:
				case ZEND_ASSIGN_SR:
				case ZEND_ASSIGN_CONCAT:
				case ZEND_ASSIGN_BW_OR:
				case ZEND_ASSIGN_BW_AND:
				case ZEND_ASSIGN_BW_XOR:
				case ZEND_ASSIGN_POW:
				case ZEND_PRE_INC:
				case ZEND_PRE_DEC:
				case ZEND_POST_INC:
				case ZEND_POST_DEC:
					if (opline->op1_type == IS_CV) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op1.var)
					}
					break;
				case ZEND_UNSET_VAR:
					if (opline->extended_value & ZEND_QUICK_SET) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = EX_VAR_TO_NUM(opline->op1.var);
						ssa_vars_count++;
					}
					break;
				case ZEND_UNSET_DIM:
				case ZEND_UNSET_OBJ:
				case ZEND_FETCH_DIM_W:
				case ZEND_FETCH_DIM_RW:
				case ZEND_FETCH_DIM_FUNC_ARG:
				case ZEND_FETCH_DIM_UNSET:
				case ZEND_FETCH_OBJ_W:
				case ZEND_FETCH_OBJ_RW:
				case ZEND_FETCH_OBJ_FUNC_ARG:
				case ZEND_FETCH_OBJ_UNSET:
					if (opline->op1_type == IS_CV) {
						ssa_ops[k].op1_def = ssa_vars_count;
						var[EX_VAR_TO_NUM(opline->op1.var)] = ssa_vars_count;
						ssa_vars_count++;
						//NEW_SSA_VAR(opline->op1.var)
					}
					break;
				default:
					break;
			}
			if (opline->result_type == IS_CV) {
				ssa_ops[k].result_def = ssa_vars_count;
				var[EX_VAR_TO_NUM(opline->result.var)] = ssa_vars_count;
				ssa_vars_count++;
				//NEW_SSA_VAR(opline->result.var)
			} else if (opline->result_type == IS_VAR ||
			           opline->result_type == IS_TMP_VAR) {
				ssa_ops[k].result_def = ssa_vars_count;
				var[EX_VAR_TO_NUM(opline->result.var)] = ssa_vars_count;
				ssa_vars_count++;
				//NEW_SSA_VAR(op_array->last_var + opline->result.var)
			}
		}
	}

	for (i = 0; i < 2; i++) {
		int succ = blocks[n].successors[i];
		if (succ >= 0) {
			zend_jit_ssa_phi *p;
			for (p = ssa_blocks[succ].phis; p; p = p->next) {
				if (p->pi == n) {
					/* e-SSA Pi */
					if (p->constraint.min_var >= 0) {
						p->constraint.min_ssa_var = var[p->constraint.min_var];
					}
					if (p->constraint.max_var >= 0) {
						p->constraint.max_ssa_var = var[p->constraint.max_var];
					}
					for (j = 0; j < blocks[succ].predecessors_count; j++) {
						p->sources[j] = var[p->var];
					}
					if (p->ssa_var < 0) {
						p->ssa_var = ssa_vars_count;
						ssa_vars_count++;
					}
				} else if (p->pi < 0) {
					/* Normal Phi */
					for (j = 0; j < blocks[succ].predecessors_count; j++)
						if (info->cfg.predecessors[blocks[succ].predecessor_offset + j] == n)
							break;
					ZEND_ASSERT(j < blocks[succ].predecessors_count);
					p->sources[j] = var[p->var];
				}
			}
			for (p = ssa_blocks[succ].phis; p && (p->pi >= 0); p = p->next) {
				if (p->pi == n) {
					zend_jit_ssa_phi *q = p->next;
					while (q) {
						if (q->pi < 0 && q->var == p->var) {
							for (j = 0; j < blocks[succ].predecessors_count; j++) {
								if (info->cfg.predecessors[blocks[succ].predecessor_offset + j] == n) {
									break;
								}
							}
							ZEND_ASSERT(j < blocks[succ].predecessors_count);
							q->sources[j] = p->ssa_var;
						}
						q = q->next;
					}
				}
			}
		}
	}

	info->ssa.vars_count = ssa_vars_count;

	j = blocks[n].children;
	while (j >= 0) {
		// FIXME: Tail call optimization?
		if (zend_jit_ssa_rename(op_array, var, j) != SUCCESS)
			return FAILURE;
		j = blocks[j].next_child;
	}

	return SUCCESS;
}

static int needs_pi(zend_op_array *op_array, zend_jit_dfg *dfg, int from, int to, int var)
{
	zend_jit_func_info *info = JIT_DATA(op_array);

	if (from == to || info->cfg.blocks[to].predecessors_count != 1) {
		zend_jit_ssa_phi *p = info->ssa.blocks[to].phis;
		while (p) {
			if (p->pi < 0 && p->var == var) {
				return 1;
			}
			p = p->next;
		}
		return 0;
	}
	return zend_bitset_in(dfg->in + (to * dfg->size), var);
}

static int add_pi(zend_jit_context *ctx, zend_op_array *op_array, zend_jit_dfg *dfg, int from, int to, int var, int min_var, int max_var, long min, long max, char underflow, char overflow, char negative)
{
	if (needs_pi(op_array, dfg, from, to, var)) {
		zend_jit_func_info *info = JIT_DATA(op_array);
		zend_jit_ssa_phi *phi = zend_jit_context_calloc(ctx,
			sizeof(zend_jit_ssa_phi) +
			sizeof(int) * info->cfg.blocks[to].predecessors_count +
			sizeof(void*) * info->cfg.blocks[to].predecessors_count, 1);

		if (!phi)
			return FAILURE;
		phi->sources = (int*)(((char*)phi) + sizeof(zend_jit_ssa_phi));
		memset(phi->sources, 0xff, sizeof(int) * info->cfg.blocks[to].predecessors_count);
		phi->use_chains = (zend_jit_ssa_phi**)(((char*)phi->sources) + sizeof(int) * info->cfg.blocks[to].predecessors_count);

		phi->pi = from;
		phi->constraint.min_var = min_var;
		phi->constraint.max_var = max_var;
		phi->constraint.min_ssa_var = -1;
		phi->constraint.max_ssa_var = -1;
		phi->constraint.range.min = min;
		phi->constraint.range.max = max;
		phi->constraint.range.underflow = underflow;
		phi->constraint.range.overflow = overflow;
		phi->constraint.negative = negative ? NEG_INIT : NEG_NONE;
		phi->var = var;
		phi->ssa_var = -1;
		phi->next = info->ssa.blocks[to].phis;
		info->ssa.blocks[to].phis = phi;
	}
	return SUCCESS;
}

static int zend_jit_build_ssa(zend_jit_context *ctx, zend_op_array *op_array)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_basic_block *blocks = info->cfg.blocks;
	zend_jit_ssa_block *ssa_blocks;
	int blocks_count = info->cfg.blocks_count;
	uint32_t set_size;
	zend_bitset tmp, gen, in;
	int *var = 0;
	int i, j, k, changed;
	zend_jit_dfg dfg;

	ZEND_JIT_CONTEXT_CALLOC(ctx, ssa_blocks, blocks_count);
	if (!ssa_blocks) {
		return FAILURE;
	}
	info->ssa.blocks = ssa_blocks;
	
	/* Compute Variable Liveness */
	dfg.vars = op_array->last_var + op_array->T;
	dfg.size = set_size = zend_bitset_len(dfg.vars);
	dfg.tmp = alloca((set_size * sizeof(zend_ulong)) * (blocks_count * 5 + 1));
	memset(dfg.tmp, 0, (set_size * sizeof(zend_ulong)) * (blocks_count * 5 + 1));
	dfg.gen = dfg.tmp + set_size;
	dfg.def = dfg.gen + set_size * blocks_count;
	dfg.use = dfg.def + set_size * blocks_count;
	dfg.in  = dfg.use + set_size * blocks_count;
	dfg.out = dfg.in  + set_size * blocks_count;
	if (zend_jit_compute_dfg(&dfg, op_array) != SUCCESS) {
		return FAILURE;
	}

	tmp = dfg.tmp;
	gen = dfg.gen;
	in  = dfg.in;

	/* SSA construction, Step 1: Propagate "gen" sets in merge points */
	do {
		changed = 0;
		for (j = 0; j < blocks_count; j++) {
			if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
				continue;
			}
			if (j >= 0 && (blocks[j].predecessors_count > 1 || j == 0)) {
				zend_bitset_copy(tmp, gen + (j * set_size), set_size);
				for (k = 0; k < blocks[j].predecessors_count; k++) {
					i = info->cfg.predecessors[blocks[j].predecessor_offset + k];
					while (i != blocks[j].idom) {
						zend_bitset_union_with_intersection(tmp, tmp, gen + (i * set_size), in + (j * set_size), set_size);
						i = blocks[i].idom;
					}
				}
				if (!zend_bitset_equal(gen + (j * set_size), tmp, set_size)) {
					zend_bitset_copy(gen + (j * set_size), tmp, set_size);
					changed = 1;
				}
			}
		}
	} while (changed);

	/* SSA construction, Step 2: Phi placement based on Dominance Frontiers */
	var = alloca(sizeof(int) * (op_array->last_var + op_array->T));
	if (!var)
		return FAILURE;
	zend_bitset_clear(tmp, set_size);

	for (j = 0; j < blocks_count; j++) {
		if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
			continue;
		}
		if (blocks[j].predecessors_count > 1) {
			zend_bitset_clear(tmp, set_size);
			if (blocks[j].flags & ZEND_BB_IRREDUCIBLE_LOOP) {
				/* Prevent any values from flowing into irreducible loops by
				   replacing all incoming values with explicit phis.  The
				   register allocator depends on this property.  */
				zend_bitset_copy(tmp, in + (j * set_size), set_size);
			} else {
				for (k = 0; k < blocks[j].predecessors_count; k++) {
					i = info->cfg.predecessors[blocks[j].predecessor_offset + k];
					while (i != blocks[j].idom) {
						zend_bitset_union_with_intersection(tmp, tmp, gen + (i * set_size), in + (j * set_size), set_size);
						i = blocks[i].idom;
					}
				}
			}

			if (!zend_bitset_empty(tmp, set_size)) {
				i = op_array->last_var + op_array->T;
				while (i > 0) {
					i--;
					if (zend_bitset_in(tmp, i)) {
						zend_jit_ssa_phi *phi = zend_jit_context_calloc(ctx,
							sizeof(zend_jit_ssa_phi) +
							sizeof(int) * blocks[j].predecessors_count +
							sizeof(void*) * blocks[j].predecessors_count, 1);

						if (!phi)
							return FAILURE;
						phi->sources = (int*)(((char*)phi) + sizeof(zend_jit_ssa_phi));
						memset(phi->sources, 0xff, sizeof(int) * blocks[j].predecessors_count);
						phi->use_chains = (zend_jit_ssa_phi**)(((char*)phi->sources) + sizeof(int) * info->cfg.blocks[j].predecessors_count);

					    phi->pi = -1;
						phi->var = i;
						phi->ssa_var = -1;
						phi->next = ssa_blocks[j].phis;
						ssa_blocks[j].phis = phi;
					}
				}
			}
		}
	}

	/* e-SSA construction: Pi placement (Pi is actually a Phi with single
	 * source and constraint).
	 * Order of Phis is importent, Pis must be placed before Phis
	 */
	for (j = 0; j < blocks_count; j++) {
		zend_op *opline = op_array->opcodes + info->cfg.blocks[j].end;
		int bt; /* successor block number if a condition is true */
		int bf; /* successor block number if a condition is false */

		if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
			continue;
		}
		/* the last instruction of basic block is conditional branch,
		 * based on comparison of CV(s)
		 */
		switch (opline->opcode) {
			case ZEND_JMPZ:
				if (info->cfg.blocks[info->cfg.blocks[j].successors[0]].start == OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes) {
					bf = info->cfg.blocks[j].successors[0];
					bt = info->cfg.blocks[j].successors[1];
				} else {
					bt = info->cfg.blocks[j].successors[0];
					bf = info->cfg.blocks[j].successors[1];
				}
				break;
			case ZEND_JMPNZ:
				if (info->cfg.blocks[info->cfg.blocks[j].successors[0]].start == OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes) {
					bt = info->cfg.blocks[j].successors[0];
					bf = info->cfg.blocks[j].successors[1];
				} else {
					bf = info->cfg.blocks[j].successors[0];
					bt = info->cfg.blocks[j].successors[1];
				}
				break;
		    case ZEND_JMPZNZ:
				if (info->cfg.blocks[info->cfg.blocks[j].successors[0]].start == OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes) {
					bf = info->cfg.blocks[j].successors[0];
					bt = info->cfg.blocks[j].successors[1];
				} else {
					bt = info->cfg.blocks[j].successors[0];
					bf = info->cfg.blocks[j].successors[1];
				}
		    	break;
			default:
				continue;
		}
		if (opline->op1_type == IS_TMP_VAR &&
		    ((opline-1)->opcode == ZEND_IS_EQUAL ||
		     (opline-1)->opcode == ZEND_IS_NOT_EQUAL ||
		     (opline-1)->opcode == ZEND_IS_SMALLER ||
		     (opline-1)->opcode == ZEND_IS_SMALLER_OR_EQUAL) &&
		    opline->op1.var == (opline-1)->result.var) {
			int  var1 = -1;
			int  var2 = -1;
			long val1 = 0;
			long val2 = 0;
//			long val = 0;

			if ((opline-1)->op1_type == IS_CV) {
				var1 = EX_VAR_TO_NUM((opline-1)->op1.var);
			} else if ((opline-1)->op1_type == IS_TMP_VAR) {
				zend_op *op = opline;
				while (op != op_array->opcodes) {
					op--;
					if (op->result_type == IS_TMP_VAR &&
					    op->result.var == (opline-1)->op1.var) {
					    if (op->opcode == ZEND_POST_DEC) {
					    	if (op->op1_type == IS_CV) {
					    		var1 = EX_VAR_TO_NUM(op->op1.var);
					    		val2--;
					    	}
					    } else if (op->opcode == ZEND_POST_INC) {
					    	if (op->op1_type == IS_CV) {
					    		var1 = EX_VAR_TO_NUM(op->op1.var);
					    		val2++;
					    	}
					    } else if (op->opcode == ZEND_ADD) {
					    	if (op->op1_type == IS_CV &&
					    	    op->op2_type == IS_CONST &&
							    Z_TYPE_P(RT_CONSTANT(op_array, op->op2)) == IS_LONG) {
					    		var1 = EX_VAR_TO_NUM(op->op1.var);
								val2 -= Z_LVAL_P(RT_CONSTANT(op_array, op->op2));						
					    	} else if (op->op2_type == IS_CV &&
					    	    op->op1_type == IS_CONST &&
							    Z_TYPE_P(RT_CONSTANT(op_array, op->op1)) == IS_LONG) {
					    		var1 = EX_VAR_TO_NUM(op->op2.var);
								val2 -= Z_LVAL_P(RT_CONSTANT(op_array, op->op1));
							}							
					    } else if (op->opcode == ZEND_SUB) {
					    	if (op->op1_type == IS_CV &&
					    	    op->op2_type == IS_CONST &&
							    Z_TYPE_P(RT_CONSTANT(op_array, op->op2)) == IS_LONG) {
					    		var1 = EX_VAR_TO_NUM(op->op1.var);
								val2 += Z_LVAL_P(RT_CONSTANT(op_array, op->op2));						
							}
					    }
					    break;
					}
				}
			}

			if ((opline-1)->op2_type == IS_CV) {
				var2 = EX_VAR_TO_NUM((opline-1)->op2.var);
			} else if ((opline-1)->op2_type == IS_TMP_VAR) {
				zend_op *op = opline;
				while (op != op_array->opcodes) {
					op--;
					if (op->result_type == IS_TMP_VAR &&
					    op->result.var == (opline-1)->op2.var) {
					    if (op->opcode == ZEND_POST_DEC) {
					    	if (op->op1_type == IS_CV) {
					    		var2 = EX_VAR_TO_NUM(op->op1.var);
					    		val1--;
					    	}
					    } else if (op->opcode == ZEND_POST_INC) {
					    	if (op->op1_type == IS_CV) {
					    		var2 = EX_VAR_TO_NUM(op->op1.var);
					    		val1++;
					    	}
					    } else if (op->opcode == ZEND_ADD) {
					    	if (op->op1_type == IS_CV &&
					    	    op->op2_type == IS_CONST &&
							    Z_TYPE_P(RT_CONSTANT(op_array, op->op2)) == IS_LONG) {
					    		var2 = EX_VAR_TO_NUM(op->op1.var);
								val1 -= Z_LVAL_P(RT_CONSTANT(op_array, op->op2));						
					    	} else if (op->op2_type == IS_CV &&
					    	    op->op1_type == IS_CONST &&
							    Z_TYPE_P(RT_CONSTANT(op_array, op->op1)) == IS_LONG) {
					    		var2 = EX_VAR_TO_NUM(op->op2.var);
								val1 -= Z_LVAL_P(RT_CONSTANT(op_array, op->op1));
							}							
					    } else if (op->opcode == ZEND_SUB) {
					    	if (op->op1_type == IS_CV &&
					    	    op->op2_type == IS_CONST &&
							    Z_TYPE_P(RT_CONSTANT(op_array, op->op2)) == IS_LONG) {
					    		var2 = EX_VAR_TO_NUM(op->op1.var);
								val1 += Z_LVAL_P(RT_CONSTANT(op_array, op->op2));						
							}
					    }
					    break;
					}
				}
			}

			if (var1 >= 0 && var2 >= 0) {
				int tmp = val1;
				val1 -= val2;
				val2 -= tmp;
			} else if (var1 >= 0 && var2 < 0) {
				if ((opline-1)->op2_type == IS_CONST &&
				    Z_TYPE_P(RT_CONSTANT(op_array, (opline-1)->op2)) == IS_LONG) {
					val2 += Z_LVAL_P(RT_CONSTANT(op_array, (opline-1)->op2));
				} else if ((opline-1)->op2_type == IS_CONST &&
				    Z_TYPE_P(RT_CONSTANT(op_array, (opline-1)->op2)) == IS_FALSE) {
					val2 += 0;
				} else if ((opline-1)->op2_type == IS_CONST &&
				    Z_TYPE_P(RT_CONSTANT(op_array, (opline-1)->op2)) == IS_TRUE) {
					val2 += 12;
			    } else {
			    	var1 = -1;
				}
			} else if (var1 < 0 && var2 >= 0) {
				if ((opline-1)->op1_type == IS_CONST &&
				    Z_TYPE_P(RT_CONSTANT(op_array, (opline-1)->op1)) == IS_LONG) {
					val1 += Z_LVAL_P(RT_CONSTANT(op_array, (opline-1)->op1));
				} else if ((opline-1)->op1_type == IS_CONST &&
				    Z_TYPE_P(RT_CONSTANT(op_array, (opline-1)->op1)) == IS_FALSE) {
					val1 += 0;
				} else if ((opline-1)->op1_type == IS_CONST &&
				    Z_TYPE_P(RT_CONSTANT(op_array, (opline-1)->op1)) == IS_TRUE) {
					val1 += 1;
			    } else {
			    	var2 = -1;
				}
			}

			if (var1 >= 0) {
				if ((opline-1)->opcode == ZEND_IS_EQUAL) {					
					if (add_pi(ctx, op_array, &dfg, j, bt, var1, var2, var2, val2, val2, 0, 0, 0) != SUCCESS)
						return FAILURE;
					if (add_pi(ctx, op_array, &dfg, j, bf, var1, var2, var2, val2, val2, 0, 0, 1) != SUCCESS)
						return FAILURE;
				} else if ((opline-1)->opcode == ZEND_IS_NOT_EQUAL) {
					if (add_pi(ctx, op_array, &dfg, j, bf, var1, var2, var2, val2, val2, 0, 0, 0) != SUCCESS)
						return FAILURE;
					if (add_pi(ctx, op_array, &dfg, j, bt, var1, var2, var2, val2, val2, 0, 0, 1) != SUCCESS)
						return FAILURE;
				} else if ((opline-1)->opcode == ZEND_IS_SMALLER) {
					if (val2 > LONG_MIN) {
						if (add_pi(ctx, op_array, &dfg, j, bt, var1, -1, var2, LONG_MIN, val2-1, 1, 0, 0) != SUCCESS)
							return FAILURE;
					}
					if (add_pi(ctx, op_array, &dfg, j, bf, var1, var2, -1, val2, LONG_MAX, 0, 1, 0) != SUCCESS)
						return FAILURE;
				} else if ((opline-1)->opcode == ZEND_IS_SMALLER_OR_EQUAL) {
					if (add_pi(ctx, op_array, &dfg, j, bt, var1, -1, var2, LONG_MIN, val2, 1, 0, 0) != SUCCESS)
						return FAILURE;
					if (val2 < LONG_MAX) {
						if (add_pi(ctx, op_array, &dfg, j, bf, var1, var2, -1, val2+1, LONG_MAX, 0, 1, 0) != SUCCESS)
							return FAILURE;
					}
				}
			}
			if (var2 >= 0) {
				if((opline-1)->opcode == ZEND_IS_EQUAL) {
					if (add_pi(ctx, op_array, &dfg, j, bt, var2, var1, var1, val1, val1, 0, 0, 0) != SUCCESS)
						return FAILURE;
					if (add_pi(ctx, op_array, &dfg, j, bf, var2, var1, var1, val1, val1, 0, 0, 1) != SUCCESS)
						return FAILURE;
				} else if ((opline-1)->opcode == ZEND_IS_NOT_EQUAL) {
					if (add_pi(ctx, op_array, &dfg, j, bf, var2, var1, var1, val1, val1, 0, 0, 0) != SUCCESS)
						return FAILURE;
					if (add_pi(ctx, op_array, &dfg, j, bt, var2, var1, var1, val1, val1, 0, 0, 1) != SUCCESS)
						return FAILURE;
				} else if ((opline-1)->opcode == ZEND_IS_SMALLER) {
					if (val1 < LONG_MAX) {
						if (add_pi(ctx, op_array, &dfg, j, bt, var2, var1, -1, val1+1, LONG_MAX, 0, 1, 0) != SUCCESS)
							return FAILURE;
					}
					if (add_pi(ctx, op_array, &dfg, j, bf, var2, -1, var1, LONG_MIN, val1, 1, 0, 0) != SUCCESS)
						return FAILURE;
				} else if ((opline-1)->opcode == ZEND_IS_SMALLER_OR_EQUAL) {
					if (add_pi(ctx, op_array, &dfg, j, bt, var2, var1, -1, val1, LONG_MAX, 0 ,1, 0) != SUCCESS)
						return FAILURE;
					if (val1 > LONG_MIN) {
						if (add_pi(ctx, op_array, &dfg, j, bf, var2, -1, var1, LONG_MIN, val1-1, 1, 0, 0) != SUCCESS)
							return FAILURE;
					}
				}
			}
		} else if (opline->op1_type == IS_TMP_VAR &&
		           ((opline-1)->opcode == ZEND_POST_INC ||
		            (opline-1)->opcode == ZEND_POST_DEC) &&
		           opline->op1.var == (opline-1)->result.var &&
		           (opline-1)->op1_type == IS_CV) {
			int var = EX_VAR_TO_NUM((opline-1)->op1.var);

			if ((opline-1)->opcode == ZEND_POST_DEC) {
				if (add_pi(ctx, op_array, &dfg, j, bf, var, -1, -1, -1, -1, 0, 0, 0) != SUCCESS)
					return FAILURE;
				if (add_pi(ctx, op_array, &dfg, j, bt, var, -1, -1, -1, -1, 0, 0, 1) != SUCCESS)
					return FAILURE;
			} else if ((opline-1)->opcode == ZEND_POST_INC) {
				if (add_pi(ctx, op_array, &dfg, j, bf, var, -1, -1, 1, 1, 0, 0, 0) != SUCCESS)
					return FAILURE;
				if (add_pi(ctx, op_array, &dfg, j, bt, var, -1, -1, 1, 1, 0, 0, 1) != SUCCESS)
					return FAILURE;
			}
		} else if (opline->op1_type == IS_VAR &&
		           ((opline-1)->opcode == ZEND_PRE_INC ||
		            (opline-1)->opcode == ZEND_PRE_DEC) &&
		           opline->op1.var == (opline-1)->result.var &&
		           (opline-1)->op1_type == IS_CV) {
			int var = EX_VAR_TO_NUM((opline-1)->op1.var);

			if ((opline-1)->opcode == ZEND_PRE_DEC) {
				if (add_pi(ctx, op_array, &dfg, j, bf, var, -1, -1, 0, 0, 0, 0, 0) != SUCCESS)
					return FAILURE;
				/* speculative */
				if (add_pi(ctx, op_array, &dfg, j, bt, var, -1, -1, 0, 0, 0, 0, 1) != SUCCESS)
					return FAILURE;
			} else if ((opline-1)->opcode == ZEND_PRE_INC) {
				if (add_pi(ctx, op_array, &dfg, j, bf, var, -1, -1, 0, 0, 0, 0, 0) != SUCCESS)
					return FAILURE;
				/* speculative */
				if (add_pi(ctx, op_array, &dfg, j, bt, var, -1, -1, 0, 0, 0, 0, 1) != SUCCESS)
					return FAILURE;
			}
		}
	}

	/* SSA construction, Step ?: Phi after Pi placement based on Dominance Frontiers */
	for (j = 0; j < blocks_count; j++) {
		if ((blocks[j].flags & ZEND_BB_REACHABLE) == 0) {
			continue;
		}
		if (blocks[j].predecessors_count > 1) {
			zend_bitset_clear(tmp, set_size);
			if (blocks[j].flags & ZEND_BB_IRREDUCIBLE_LOOP) {
				/* Prevent any values from flowing into irreducible loops by
				   replacing all incoming values with explicit phis.  The
				   register allocator depends on this property.  */
				zend_bitset_copy(tmp, in + (j * set_size), set_size);
			} else {
				for (k = 0; k < blocks[j].predecessors_count; k++) {
					i = info->cfg.predecessors[blocks[j].predecessor_offset + k];
					while (i != blocks[j].idom) {
						zend_jit_ssa_phi *p = ssa_blocks[i].phis;
						while (p) {
							if (p) {
								if (p->pi >= 0) {
									if (zend_bitset_in(in + (j * set_size), p->var) &&
									    !zend_bitset_in(gen + (i * set_size), p->var)) {
										zend_bitset_incl(tmp, p->var);
									}
								} else {
									zend_bitset_excl(tmp, p->var);
								}
							}
							p = p->next;
						}
						i = blocks[i].idom;
					}
				}
			}

			if (!zend_bitset_empty(tmp, set_size)) {
				i = op_array->last_var + op_array->T;
				while (i > 0) {
					i--;
					if (zend_bitset_in(tmp, i)) {
						zend_jit_ssa_phi **pp = &ssa_blocks[j].phis;
						while (*pp) {
							if ((*pp)->pi <= 0 && (*pp)->var == i) {
								break;
							}
							pp = &(*pp)->next;
						}
						if (*pp == NULL) {
							zend_jit_ssa_phi *phi = zend_jit_context_calloc(ctx,
								sizeof(zend_jit_ssa_phi) +
								sizeof(int) * blocks[j].predecessors_count +
								sizeof(void*) * blocks[j].predecessors_count, 1);

							if (!phi)
								return FAILURE;
							phi->sources = (int*)(((char*)phi) + sizeof(zend_jit_ssa_phi));
							memset(phi->sources, 0xff, sizeof(int) * blocks[j].predecessors_count);
							phi->use_chains = (zend_jit_ssa_phi**)(((char*)phi->sources) + sizeof(int) * info->cfg.blocks[j].predecessors_count);

						    phi->pi = -1;
							phi->var = i;
							phi->ssa_var = -1;
							phi->next = NULL;
							*pp = phi;
						}
					}
				}
			}
		}
	}

	if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_PHI) {
		zend_jit_dump(op_array, JIT_DUMP_PHI_PLACEMENT);
	}

	/* SSA construction, Step 3: Renaming */
	ZEND_JIT_CONTEXT_CALLOC(ctx, info->ssa.ops, op_array->last);
	memset(info->ssa.ops, 0xff, op_array->last * sizeof(zend_jit_ssa_op));
	memset(var, 0xff, (op_array->last_var + op_array->T) * sizeof(int));
	/* Create uninitialized SSA variables for each CV */
	for (j = 0; j < op_array->last_var; j++) {
		var[j] = j;
	}
	info->ssa.vars_count = op_array->last_var;
	if (zend_jit_ssa_rename(op_array, var, 0) != SUCCESS)
		return FAILURE;

	if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_SSA) {
		zend_jit_dump(op_array, JIT_DUMP_SSA);
	}

	return SUCCESS;
}

int zend_jit_parse_ssa(zend_jit_context *ctx, zend_op_array *op_array)
{
	zend_jit_func_info *info = JIT_DATA(op_array);

	if (info->flags & ZEND_JIT_FUNC_TOO_DYNAMIC)
		abort();

	/* Compute Dominators Tree */
	if (zend_jit_compute_dominators_tree(op_array) != SUCCESS)
		return FAILURE;

	/* Identify reducible and irreducible loops */
	if (zend_jit_identify_loops(op_array) != SUCCESS)
		return FAILURE;

	if (zend_jit_build_ssa(ctx, op_array) != SUCCESS)
		return FAILURE;

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */