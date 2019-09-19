#!/usr/bin/env python3

"""
Generate f32x4 [abs, min, max] cases.
"""

from simd_f32x4_arith import Simdf32x4ArithmeticCase


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
        return '(v128.const {} {})'.format(lane, ' '.join([str(val)] * 4))

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