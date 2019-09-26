#!/usr/bin/env python3

"""
Generate f32x4 [abs, min, max] cases.
"""

from simd_f32x4_arith import Simdf32x4ArithmeticCase
from simd import SIMD
from test_assert import AssertReturn


def binary_op(op: str, p1: str, p2: str) -> str:
    """Binary operation on p1 and p2 with the operation specified by op

    :param op: min, max,
    :param p1: float number in hex
    :param p2: float number in hex
    :return:
    """
    f1 = float.fromhex(p1)
    f2 = float.fromhex(p2)

    if '-nan' in [p1, p2] and 'nan' in [p1, p2]:
        return p1

    if 'nan' in [p1, p2]:
        return 'nan'

    if '-nan' in [p1, p2]:
        return '-nan'

    if op == 'min':
        if '-0x0p+0' in [p1, p2] and '0x0p+0' in [p1, p2]:
            return '-0x0p+0'
        result = min(f1, f2)

    elif op == 'max':
        if '-0x0p+0' in [p1, p2] and '0x0p+0' in [p1, p2]:
            return '0x0p+0'
        result = max(f1, f2)

    else:
        raise Exception('Unknown binary operation: {}'.format(op))

    return result.hex()


def unary_op(op: str, p1: str) -> str:
    """Unnary operation on p1 with the operation specified by op

    :param op: abs,
    :param p1: float number in hex
    :return:
    """
    f1 = float.fromhex(p1)
    if op == 'abs':
        return abs(f1).hex()

    raise Exception('Unknown unary operation: {}'.format(op))


class Simdf32x4Case(Simdf32x4ArithmeticCase):
    UNARY_OPS = ('abs',)
    BINARY_OPS = ('min', 'max',)

    FLOAT_NUMBERS = (
        '0x0p+0', '-0x0p+0', '0x1p-149', '-0x1p-149', '0x1p-126', '-0x1p-126', '0x1p-1', '-0x1p-1', '0x1p+0', '-0x1p+0',
        '0x1.921fb6p+2', '-0x1.921fb6p+2', '0x1.fffffep+127', '-0x1.fffffep+127', 'inf', '-inf'
    )

    NAN_NUMBERS = ('nan', '-nan', 'nan:0x200000', '-nan:0x200000')
    binary_params_template = ('({} (invoke "{}" ', '{}', '{})', '{})')
    unary_param_template = ('({} (invoke "{}" ', '{})', '{})')
    binary_nan_template = ('({} (invoke "{}" ', '{}', '{}))')
    unary_nan_template = ('({} (invoke "{}" ', '{}))')

    def full_op_name(self, op_name):
        return self.LANE_TYPE + '.' + op_name

    @staticmethod
    def v128_const(lane, val):

        return SIMD().v128_const(val, lane)

    def gen_test_fn_template(self):

        # Get function code
        template = Simdf32x4ArithmeticCase.gen_test_fn_template(self)

        # Function template
        tpl_func = '  (func (export "{}") (result v128) ({} {}{}))'

        # Const data for min and max
        lst_instr_with_const = [
            [
                [['0', '1', '2', '-3'], ['0', '2', '1', '3']],
                [['0', '1', '1', '-3'], ['0', '2', '2', '3']]
            ],
            [
                [['0', '1', '2', '3'], ['0', '1', '2', '3']],
                [['0', '1', '2', '3'], ['0', '1', '2', '3']]
            ],
            [
                [['0x00', '0x01', '0x02', '0x80000000'], ['0x00', '0x02', '0x01', '2147483648']],
                [['0x00', '0x01', '0x01', '0x80000000'], ['0x00', '0x02', '0x02', '2147483648']]
            ],
            [
                [['0x00', '0x01', '0x02', '0x80000000'], ['0x00', '0x01', '0x02', '0x80000000']],
                [['0x00', '0x01', '0x02', '0x80000000'], ['0x00', '0x01', '0x02', '0x80000000']]
            ]
        ]

        # Assert data
        lst_assert = {}

        # Generate func and assert
        for op in self.BINARY_OPS:

            op_name = self.full_op_name(op)

            for case_data in lst_instr_with_const:

                func_name = "{}_with_const_{}".format(op_name, len(template)-6)
                template.insert(len(template)-1,
                                tpl_func.format(func_name, op_name,
                                                self.v128_const('f32x4', case_data[0][0]),
                                                ' ' + self.v128_const('f32x4', case_data[0][1])))

                ret_idx = 0 if op == 'min' else 1

                if op not in lst_assert:
                    lst_assert[op] = []

                lst_assert[op].append([func_name, case_data[1][ret_idx]])

        # Generate func for abs
        op_name = self.full_op_name('abs')
        func_name = "{}_with_const_{}".format(op_name, len(template)-6)
        template.insert(len(template)-1,
                        tpl_func.format(func_name, op_name,
                                        self.v128_const('f32x4', ['-0', '-1', '-2', '-3']), ''))

        for key in lst_assert:
            for case_data in lst_assert[key]:
                template.append(str(AssertReturn(case_data[0], [], self.v128_const('f32x4', case_data[1]))))

        template.append(str(AssertReturn(func_name, [], self.v128_const('f32x4', ['0', '1', '2', '3']))))

        return template

    @property
    def combine_ternary_arith_test_data(self):
        return {
            'min-max': [
                ['1.125'] * 4, ['0.25'] * 4, ['0.125'] * 4, ['0.125'] * 4
            ],
            'max-min': [
                ['1.125'] * 4, ['0.25'] * 4, ['0.125'] * 4, ['0.25'] * 4
            ]
        }

    @property
    def combine_binary_arith_test_data(self):
        return {
            'min-abs': [
                ['-1.125'] * 4, ['0.125'] * 4, ['0.125'] * 4
            ],
            'max-abs': [
                ['-1.125'] * 4, ['0.125'] * 4, ['1.125'] * 4
            ]
        }

    def get_normal_case(self):
        """Normal test cases from WebAssembly core tests, 4 assert statements:
            assert_return
            assert_return_canonical_nan
            assert_return_arithmetic_nan
            assert_malformed
        """
        cases = []
        binary_test_data = []
        unary_test_data = []

        for op in self.BINARY_OPS:
            op_name = self.full_op_name(op)
            for p1 in self.FLOAT_NUMBERS:
                for p2 in self.FLOAT_NUMBERS:
                    result = binary_op(op, p1, p2)
                    if 'nan' not in result:
                        # Normal floating point numbers as the results
                        binary_test_data.append(['assert_return', op_name, p1, p2, result])
                    else:
                        # Since the results contain the 'nan' string, it should be in the
                        # assert_return_canonical_nan statements
                        binary_test_data.append(['assert_return_canonical_nan_f32x4', op_name, p1, p2])

            # assert_return_canonical_nan and assert_return_arithmetic_nan cases
            for p1 in self.NAN_NUMBERS:
                for p2 in self.FLOAT_NUMBERS:
                    if 'nan:' in p1 or 'nan:' in p2:
                        # When the arguments contain 'nan:', always use assert_return_arithmetic_nan
                        # statements for the cases. Since there 2 parameters for binary operation and
                        # the order of the parameters matter. Different order makes different cases.
                        binary_test_data.append(['assert_return_arithmetic_nan_f32x4', op_name, p1, p2])
                        binary_test_data.append(['assert_return_arithmetic_nan_f32x4', op_name, p2, p1])
                    else:
                        # No 'nan' string found, then it should be assert_return_canonical_nan.
                        binary_test_data.append(['assert_return_canonical_nan_f32x4', op_name, p1, p2])
                        binary_test_data.append(['assert_return_canonical_nan_f32x4', op_name, p2, p1])
                for p2 in self.NAN_NUMBERS:
                    # Both parameters contain 'nan', then there must be no assert_return.
                    if 'nan:' in p1 or 'nan:' in p2:
                        binary_test_data.append(['assert_return_arithmetic_nan_f32x4', op_name, p1, p2])
                    else:
                        binary_test_data.append(['assert_return_canonical_nan_f32x4', op_name, p1, p2])

        for case in binary_test_data:
            cases.append(self.single_binary_test(case))

        # Test different lanes go through different if-then clauses and opposite signs of zero
        lst_specific_test_data = [
            '\n;; Test different lanes go through different if-then clauses',
            [
                'f32x4.min',
                [['nan', '0', '0', '1'], ['0', '-nan', '1', '0']],
                [['nan', '-nan', '0', '0']],
                ['f32x4', 'f32x4', 'f32x4']
            ],
            [
                'f32x4.min',
                [['nan', '0', '0', '0'], ['0', '-nan', '1', '0']],
                [['nan', '-nan', '0', '0']],
                ['f32x4', 'f32x4', 'f32x4']
            ],
            [
                'f32x4.max',
                [['nan', '0', '0', '1'], ['0', '-nan', '1', '0']],
                [['nan', '-nan', '1', '1']],
                ['f32x4', 'f32x4', 'f32x4']
            ],
            [
                'f32x4.max',
                [['nan', '0', '0', '0'], ['0', '-nan', '1', '0']],
                [['nan', '-nan', '1', '0']],
                ['f32x4', 'f32x4', 'f32x4']
            ],
            '\n;; Test opposite signs of zero',
            [
                'f32x4.min',
                [['0', '0', '-0', '+0'], ['+0', '-0', '+0', '-0']],
                [['0', '-0', '-0', '-0']],
                ['f32x4', 'f32x4', 'f32x4']
            ],
            [
                'f32x4.min',
                [['-0', '-0', '-0', '-0'], ['+0', '+0', '+0', '+0']],
                [['-0', '-0', '-0', '-0']],
                ['f32x4', 'f32x4', 'f32x4']
            ],
            [
                'f32x4.max',
                [['0', '0', '-0', '+0'], ['+0', '-0', '+0', '-0']],
                [['0', '0', '0', '0']],
                ['f32x4', 'f32x4', 'f32x4']
            ],
            [
                'f32x4.max',
                [['-0', '-0', '-0', '-0'], ['+0', '+0', '+0', '+0']],
                [['+0', '+0', '+0', '+0']],
                ['f32x4', 'f32x4', 'f32x4']
            ],
            '\n'
        ]

        # Generate test case for if-then-else branch and opposite signs of zero
        for case_data in lst_specific_test_data:

            if isinstance(case_data, str):
                cases.append(case_data)
                continue

            cases.append(str(AssertReturn(case_data[0],
                                          [self.v128_const(case_data[3][0], case_data[1][0]),
                                           self.v128_const(case_data[3][1], case_data[1][1])],
                                          self.v128_const(case_data[3][2], case_data[2][0]))))

        for p in self.FLOAT_NUMBERS:
            op_name = self.full_op_name('abs')
            result = unary_op('abs', p)
            # Abs operation is valid for all the floating point numbers
            unary_test_data.append(['assert_return', op_name, p, result])

        for case in unary_test_data:
            cases.append(self.single_unary_test(case))

        self.get_unknow_operator_case(cases)

        return '\n'.join(cases)

    def get_unknow_operator_case(self, cases):
        """Unknow operator cases.
        """

        tpl_assert = "(assert_malformed (module quote \"(memory 1) (func (result v128) " \
                     "({}.{} {}))\") \"unknown operator\")"

        unknow_op_cases = ['\n\n;; Unknown operators\n']
        cases.extend(unknow_op_cases)

        for lane_type in ['i8x16', 'i16x8', 'i32x4']:

            for op in self.UNARY_OPS:
                cases.append(tpl_assert.format(lane_type, op, self.v128_const('i32x4', '0')))

            for op in self.BINARY_OPS:
                cases.append(tpl_assert.format(lane_type, op, ' '.join([self.v128_const('i32x4', '0')]*2)))

    def gen_test_cases(self):
        wast_filename = '../simd_{lane_type}.wast'.format(lane_type=self.LANE_TYPE)
        with open(wast_filename, 'w') as fp:
            txt_test_case = self.get_all_cases()
            txt_test_case = txt_test_case.replace('f32x4 arithmetic', 'f32x4 [abs, min, max]')
            fp.write(txt_test_case)


def gen_test_cases():
    simd_f32x4_case = Simdf32x4Case()
    simd_f32x4_case.gen_test_cases()


if __name__ == '__main__':
    gen_test_cases()