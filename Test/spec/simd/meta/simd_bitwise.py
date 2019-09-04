#!/usr/bin/env python3

"""
This file is used for generating bitwise test cases
"""

from simd import SIMD
from test_assert import AssertReturn


class SimdBitWise(SIMD):
    """
    Generate common tests
    """

    # Test case template
    CASE_TXT = """;; Test all the bitwise operators on major boundary values and all special values.

(module
  (func (export "not") (param $0 v128) (result v128) (v128.not (local.get $0)))
  (func (export "and") (param $0 v128) (param $1 v128) (result v128) (v128.and (local.get $0) (local.get $1)))
  (func (export "or") (param $0 v128) (param $1 v128) (result v128) (v128.or (local.get $0) (local.get $1)))
  (func (export "xor") (param $0 v128) (param $1 v128) (result v128) (v128.xor (local.get $0) (local.get $1)))
  (func (export "bitselect") (param $0 v128) (param $1 v128) (param $2 v128) (result v128)
    (v128.bitselect (local.get $0) (local.get $1) (local.get $2))
  )
)
{normal_case}"""

    @staticmethod
    def init_case_data(case_data):
        """
        Rearrange const data into standard format
        e.g. [0][i32x4] => (v128.const i32x4 0 0 0 0)
             [0][i32]   => (i32.const 0)
        """

        s_i = SIMD()

        lst_i_p_r = []

        for item in case_data:
            # Recognize '#' as a commentary
            if item[0] == '#':
                comment = '\n' if len(item[1]) == 0 else '\n;; {}'.format(item[1])
                lst_i_p_r.append(['#', comment])
                continue

            # Params: instruction: instruction name;
            #         params: param for instruction;
            #         rets: excepted result;
            #         lane_type: lane type for param and ret
            instruction, params, rets, lane_type = item

            p_const_list = []
            for idx, param in enumerate(params):
                p_const_list.append(s_i.v128_const(param, lane_type[idx]))

            r_const_list = []
            for idx, ret in enumerate(rets):
                r_const_list.append(s_i.v128_const(ret, lane_type[idx + len(params)]))

            lst_i_p_r.append([instruction, p_const_list, r_const_list])

        return lst_i_p_r

    # Generate normal case with test datas
    def get_normal_case(self):
        """
        Generate normal case with test data
        """

        lst_i_p_r = self.init_case_data(self.get_case_data())

        cases = []
        for ipr in lst_i_p_r:

            if ipr[0] == '#':
                cases.append(ipr[1])
                continue

            cases.append(str(AssertReturn(ipr[0],
                                          ipr[1],
                                          ipr[2])))

        return '\n'.join(cases)

    def get_invalid_case(self):
        """
        Generate invalid case with test data
        """

        case_data = [
            # i8x16
            ['#', 'Type check'],
            ['#', ''],

            ['#', 'not'],
            ["v128.not", ['0'], [], ['i32']],

            ['#', 'and'],
            ["v128.and", ['0', '0'], [], ['i32', 'i32x4']],
            ["v128.and", ['0', '0'], [], ['i32x4', 'i32']],
            ["v128.and", ['0', '0'], [], ['i32', 'i32']],

            ['#', 'or'],
            ["v128.or", ['0', '0'], [], ['i32', 'i32x4']],
            ["v128.or", ['0', '0'], [], ['i32x4', 'i32']],
            ["v128.or", ['0', '0'], [], ['i32', 'i32']],

            ['#', 'xor'],
            ["v128.xor", ['0', '0'], [], ['i32', 'i32x4']],
            ["v128.xor", ['0', '0'], [], ['i32x4', 'i32']],
            ["v128.xor", ['0', '0'], [], ['i32', 'i32']],

            ['#', 'bitselect'],
            ["v128.bitselect", ['0', '0', '0'], [], ['i32', 'i32x4', 'i32x4']],
            ["v128.bitselect", ['0', '0', '0'], [], ['i32x4', 'i32x4', 'i32']],
            ["v128.bitselect", ['0', '0', '0'], [], ['i32', 'i32', 'i32']]
        ]

        lst_ipr = self.init_case_data(case_data)

        str_invalid_case_func_tpl = '\n(assert_invalid (module (func (result v128)' \
                                    ' ({} {}))) "type mismatch")'

        lst_invalid_case_func = []

        for ipr in lst_ipr:

            if ipr[0] == '#':
                lst_invalid_case_func.append(ipr[1])
                continue
            else:
                lst_invalid_case_func.append(
                    str_invalid_case_func_tpl.format(ipr[0], ' '.join(ipr[1]))
                )

        return '\n{}\n'.format(''.join(lst_invalid_case_func))

    def get_combination_case(self):
        """
        Generate combination case with test data
        """

        str_in_block_case_func_tpl = '\n  (func (export "{}-in-block")' \
                                     '\n    (block' \
                                     '\n      (drop' \
                                     '\n        (block (result v128)' \
                                     '\n          ({}' \
                                     '{}' \
                                     '\n          )' \
                                     '\n        )' \
                                     '\n      )' \
                                     '\n    )' \
                                     '\n  )'
        str_nested_case_func_tpl = '\n  (func (export "nested-{}")' \
                                   '\n    (drop' \
                                   '\n      ({}' \
                                   '{}' \
                                   '\n      )' \
                                   '\n    )' \
                                   '\n  )'

        case_data = [
            ["v128.not", ['0'], [], ['i32']],
            ["v128.and", ['0', '1'], [], ['i32', 'i32']],
            ["v128.or", ['0', '1'], [], ['i32', 'i32']],
            ["v128.xor", ['0', '1'], [], ['i32', 'i32']],
            ["v128.bitselect", ['0', '1', '2'], [], ['i32', 'i32', 'i32']],
        ]
        lst_ipr = self.init_case_data(case_data)

        lst_in_block_case_func = []
        lst_nested_case_func = []
        lst_in_block_case_assert = []
        lst_nested_case_assert = []

        for ipr in lst_ipr:

            lst_block = ['\n            (block (result v128) (v128.load {}))'.format(x) for x in ipr[1]]
            lst_in_block_case_func.append(
                str_in_block_case_func_tpl.format(ipr[0], ipr[0], ''.join(lst_block))
            )

            tpl_1 = '\n        ({}' \
                    '{}' \
                    '\n        )'
            tpl_2 = '\n          ({}' \
                    '{}' \
                    '\n          )'
            tpl_3 = '\n            (v128.load {})'

            lst_tpl_3 = [tpl_3.format(x) for x in ipr[1]]
            lst_tpl_2 = [tpl_2.format(ipr[0], ''.join(lst_tpl_3))] * len(ipr[1])
            lst_tpl_1 = [tpl_1.format(ipr[0], ''.join(lst_tpl_2))] * len(ipr[1])

            lst_nested_case_func.append(
                str_nested_case_func_tpl.format(ipr[0], ipr[0], ''.join(lst_tpl_1))
            )

            lst_in_block_case_assert.append('\n(assert_return (invoke "{}-in-block"))'.format(ipr[0]))
            lst_nested_case_assert.append('\n(assert_return (invoke "nested-{}"))'.format(ipr[0]))

        return '\n;; Combination\n' \
               '\n(module (memory 1)' \
               '{}' \
               '{}' \
               '\n  (func (export "as-param")' \
               '\n    (drop' \
               '\n      (v128.or' \
               '\n        (v128.and' \
               '\n          (v128.not' \
               '\n            (v128.load (i32.const 0))' \
               '\n          )' \
               '\n          (v128.not' \
               '\n            (v128.load (i32.const 1))' \
               '\n          )' \
               '\n        )' \
               '\n        (v128.xor' \
               '\n          (v128.bitselect' \
               '\n            (v128.load (i32.const 0))' \
               '\n            (v128.load (i32.const 1))' \
               '\n            (v128.load (i32.const 2))' \
               '\n          )' \
               '\n          (v128.bitselect' \
               '\n            (v128.load (i32.const 0))' \
               '\n            (v128.load (i32.const 1))' \
               '\n            (v128.load (i32.const 2))' \
               '\n          )' \
               '\n        )' \
               '\n      )' \
               '\n    )' \
               '\n  )' \
               '\n)' \
               '{}' \
               '{}' \
               '\n(assert_return (invoke "as-param"))\n'.format(''.join(lst_in_block_case_func),
                                                                ''.join(lst_nested_case_func),
                                                                ''.join(lst_in_block_case_assert),
                                                                ''.join(lst_nested_case_assert))

    def get_all_cases(self):
        """
        generate all test cases
        """

        case_data = {'normal_case': self.get_normal_case()}

        # Add tests for unkonow operators for i32x4
        return self.CASE_TXT.format(**case_data) + self.get_invalid_case() + self.get_combination_case()

    def get_case_data(self):
        """
        Overload base class method and set test data for bitwise.
        """
        return [
            # i8x16
            ['#', 'bitwise operations'],
            ['#', 'i8x16'],
            ["not", ['0'], ['-1'], ['i8x16', 'i8x16']],
            ["not", ['-1'], ['0'], ['i8x16', 'i8x16']],
            ["not", [['-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0']],
                    [['0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                    ['i8x16', 'i8x16']],
            ["not", [['0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                    [['-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0']],
                    ['i8x16', 'i8x16']],
            ["not", ['0x55'], ['0xAA'], ['i8x16', 'i8x16']],
            ["not", ['204'], ['51'], ['i8x16', 'i8x16']],
            ["and", [['0', '0', '-1', '-1', '0', '0', '-1', '-1', '0', '0', '-1', '-1', '0', '0', '-1', '-1'],
                     ['0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                    [['0', '0', '0', '-1', '0', '0', '0', '-1', '0', '0', '0', '-1', '0', '0', '0', '-1']],
                    ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['0', '0'], ['0'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['0', '-1'], ['0'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['0', '0xFF'], ['0'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['1', '1'], ['1'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['255', '85'], ['85'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['255', '128'], ['128'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['170', ['10', '128', '5', '165', '10', '128', '5', '165',
                             '10', '128', '5', '165', '10', '128', '5', '165']],
                    [['10', '128', '0', '160', '10', '128', '0', '160',
                      '10', '128', '0', '160', '10', '128', '0', '160']],
                    ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['0xFF', '0x55'], ['0x55'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['0xFF', '0xAA'], ['0xAA'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['0xFF', '0x00'], ['0x00'], ['i8x16', 'i8x16', 'i8x16']],
            ["and", ['0x55',
                     ['0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5',
                      '0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5']],
                    ['0x55'],
                    ['i8x16', 'i8x16', 'i8x16']],
            ["or", [['0', '0', '-1', '-1', '0', '0', '-1', '-1', '0', '0', '-1', '-1', '0', '0', '-1', '-1'],
                    ['0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                   [['0', '-1', '-1', '-1', '0', '-1', '-1', '-1', '0', '-1', '-1', '-1', '0', '-1', '-1', '-1']],
                   ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['0', '0'], ['0'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['0', '-1'], ['-1'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['0', '0xFF'], ['0xFF'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['1', '1'], ['1'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['255', '85'], ['255'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['255', '128'], ['255'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['170', ['10', '128', '5', '165', '10', '128', '5', '165',
                            '10', '128', '5', '165', '10', '128', '5', '165']],
                   [['170', '175', '170', '175', '170', '175', '170', '175']],
                   ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['0xFF', '0x55'], ['0xFF'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['0xFF', '0xAA'], ['0xFF'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['0xFF', '0x00'], ['0xFF'], ['i8x16', 'i8x16', 'i8x16']],
            ["or", ['0x55', ['0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5',
                             '0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5']],
                   [['0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5',
                     '0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5']],
                   ['i8x16', 'i8x16', 'i8x16']],
            ["xor", [['0', '-1', '0', '-1', '0', '-1', '0', '-1'],
                     ['0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                    [['0', '-1', '-1', '0', '0', '-1', '-1', '0', '0', '-1', '-1', '0', '0', '-1', '-1', '0']],
                    ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['0', '0'], ['0'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['0', '-1'], ['-1'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['0', '0xFF'], ['0xFF'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['1', '1'], ['0'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['255', '85'], ['170'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['255', '128'], ['127'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['170', ['10', '128', '5', '165', '10', '128', '5', '165',
                             '10', '128', '5', '165', '10', '128', '5', '165']],
                    [['160', '42', '175', '15', '160', '42', '175', '15',
                      '160', '42', '175', '15', '160', '42', '175', '15']],
                    ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['0xFF', '0x55'], ['0xAA'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['0xFF', '0xAA'], ['0x55'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['0xFF', '0x00'], ['0xFF'], ['i8x16', 'i8x16', 'i8x16']],
            ["xor", ['0x55', ['0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5',
                              '0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5']],
                    [['0x00', '0xAA', '0x0A', '0xA0', '0x00', '0xAA', '0x0A', '0xA0',
                      '0x00', '0xAA', '0x0A', '0xA0', '0x00', '0xAA', '0x0A', '0xA0']],
                    ['i8x16', 'i8x16', 'i8x16']],
            ["bitselect", ['0xAA', '0xBB',
                           ['0x00', '0x11', '0x23', '0x45', '0xF0', '0x0F', '0xFF',
                            '0xFF', '16', '17', '32', '33', '0xBB', '0xAA', '0xBB', '0xAA']],
                          [['0xBB', '0xAA', '0xBA', '0xBA', '0xAB', '0xBA', '0xAA', '0xAA',
                            '0xAB', '0xAA', '0xBB', '0xBA', '0xAA', '0xBB', '0xAA', '0xBB']],
                          ['i8x16', 'i8x16', 'i8x16', 'i8x16']],
            ["bitselect", ['0xAA', '0xBB', '0x00'], ['0xBB'], ['i8x16', 'i8x16', 'i8x16', 'i8x16']],
            ["bitselect", ['0xAA', '0xBB', '0x11'], ['0xAA'], ['i8x16', 'i8x16', 'i8x16', 'i8x16']],
            ["bitselect", ['0xAA', '0xBB',
                           ['0x01', '0x23', '0x45', '0x67', '0x89', '0xAB', '0xCD', '0xEF',
                            '0xFE', '0xDC', '0xBA', '0x98', '0x76', '0x54', '0x32', '0x10']],
                          [['0xBA', '0xBA', '0xBA', '0xBA', '0xBA', '0xBA', '0xBA', '0xBA',
                            '0xAB', '0xAB', '0xAB', '0xAB', '0xAB', '0xAB', '0xAB', '0xAB']],
                          ['i8x16', 'i8x16', 'i8x16', 'i8x16']],
            ["bitselect", ['0xAA', '0x55',
                           ['0x01', '0x23', '0x45', '0x67', '0x89', '0xAB', '0xCD', '0xEF',
                            '0xFE', '0xDC', '0xBA', '0x98', '0x76', '0x54', '0x32', '0x10']],
                          [['0x54', '0x76', '0x10', '0x32', '0xDC', '0xFE', '0x98', '0xBA',
                            '0xAB', '0x89', '0xEF', '0xCD', '0x23', '0x01', '0x67', '0x45']],
                          ['i8x16', 'i8x16', 'i8x16', 'i8x16']],
            ["bitselect", ['0xAA', '0x55', ['0x55', '0xAA', '0x00', '0xFF']],
                          [['0x00', '0x00', '0x00', '0x00', '0xFF', '0xFF', '0xFF', '0xFF',
                            '0x55', '0x55', '0x55', '0x55', '0xAA', '0xAA', '0xAA', '0xAA']],
                          ['i8x16', 'i8x16', 'i8x16', 'i8x16']],

            # i16x8
            ['#', 'i16x8'],
            ["not", ['0'], ['-1'], ['i16x8', 'i16x8']],
            ["not", ['-1'], ['0'], ['i16x8', 'i16x8']],
            ["not", [['-1', '0', '-1', '0', '-1', '0', '-1', '0']],
                    [['0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                    ['i16x8', 'i16x8']],
            ["not", [['0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                    [['-1', '0', '-1', '0', '-1', '0', '-1', '0']],
                    ['i16x8', 'i16x8']],
            ["not", ['0x5555'], ['0xAAAA'], ['i16x8', 'i16x8']],
            ["not", ['204'], ['65331'], ['i16x8', 'i16x8']],
            ["and", [['0', '-1', '0', '-1'], ['0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                    [['0', '0', '0', '-1', '0', '0', '0', '-1']],
                    ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['0', '0'], ['0'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['0', '-1'], ['0'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['0', '0xFFFF'], ['0'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['1', '1'], ['1'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['255', '85'], ['85'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['255', '128'], ['128'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['170', ['10', '128', '5', '165', '10', '128', '5', '165']],
                    [['10', '128', '0', '160', '10', '128', '0', '160']], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['0xFFFF', '0x5555'], ['0x5555'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['0xFFFF', '0xAAAA'], ['0xAAAA'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['0xFFFF', '0x00'], ['0x00'], ['i16x8', 'i16x8', 'i16x8']],
            ["and", ['0x5555', ['0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5']],
                    ['0x0055'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", [['0', '-1', '0', '-1'], ['0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                   [['0', '-1', '-1', '-1', '0', '-1', '-1', '-1']], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['0', '0'], ['0'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['0', '-1'], ['-1'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['0', '0xFFFF'], ['0xFFFF'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['1', '1'], ['1'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['255', '85'], ['255'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['255', '128'], ['255'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['170', ['10', '128', '5', '165', '10', '128', '5', '165']],
                   [['170', '170', '175', '175', '170', '170', '175', '175']], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['0xFFFF', '0x5555'], ['0xFFFF'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['0xFFFF', '0xAAAA'], ['0xFFFF'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['0xFFFF', '0x00'], ['0xFFFF'], ['i16x8', 'i16x8', 'i16x8']],
            ["or", ['0x5555', ['0x55', '0xFF', '0x5F', '0xF5', '0x55', '0xFF', '0x5F', '0xF5']],
                   [['0x5555', '0x55FF', '0x555F', '0x55F5', '0x5555', '0x55FF', '0x555F', '0x55F5']],
                   ['i16x8', 'i16x8', 'i16x8']],
            ["xor", [['0', '-1', '0', '-1'], ['0', '-1', '0', '-1', '0', '-1', '0', '-1']],
                    [['0', '-1', '-1', '0', '0', '-1', '-1', '0']], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['0', '0'], ['0'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['0', '-1'], ['-1'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['0', '0xFFFF'], ['0xFFFF'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['1', '1'], ['0'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['255', '85'], ['170'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['255', '128'], ['127'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['170', ['10', '128', '5', '165', '10', '128', '5', '165']],
                    [['160', '42', '175', '15', '160', '42', '175', '15']], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['0xFFFF', '0x5555'], ['0xAAAA'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['0xFFFF', '0xAAAA'], ['0x5555'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['0xFFFF', '0x00'], ['0xFFFF'], ['i16x8', 'i16x8', 'i16x8']],
            ["xor", ['0x5555', ['0x0055', '0x00FF', '0x005F', '0x00F5', '0x0055', '0x00FF', '0x005F', '0x00F5']],
                    [['0x5500', '0x55AA', '0x550A', '0x55A0', '0x5500', '0x55AA', '0x550A', '0x55A0']],
                    ['i16x8', 'i16x8', 'i16x8']],
            ["bitselect", ['0xAAAA', '0xBBBB', ['0x0011', '0x2345', '0xF00F', '0xFFFF', '4113', '8225', '0xBBAA', '0xBBAA']],
                          [['0xBBAA', '0xBABA', '0xABBA', '0xAAAA', '0xABAA', '0xBBBA', '0xAABB', '0xAABB']],
                          ['i16x8', 'i16x8', 'i16x8', 'i16x8']],
            ["bitselect", ['0xAAAA', '0xBBBB', '0x00'], ['0xBBBB'], ['i16x8', 'i16x8', 'i16x8', 'i16x8']],
            ["bitselect", ['0xAAAA', '0xBBBB', '0x1111'], ['0xAAAA'], ['i16x8', 'i16x8', 'i16x8', 'i16x8']],
            ["bitselect", ['0xAAAA', '0xBBBB',
                           ['0x0123', '0x4567', '0x89AB', '0xCDEF', '0xFEDC', '0xBA98', '0x7654', '0x3210']],
                          [['0xBABA', '0xBABA', '0xBABA', '0xBABA', '0xABAB', '0xABAB', '0xABAB', '0xABAB']],
                          ['i16x8', 'i16x8', 'i16x8', 'i16x8']],
            ["bitselect", ['0xAAAA', '0x5555', ['0x0123', '0x4567', '0x89AB', '0xCDEF', '0xFEDC', '0xBA98', '0x7654', '0x3210']],
                          [['0x5476', '0x1032', '0xDCFE', '0x98BA', '0xAB89', '0xEFCD', '0x2301', '0x6745']],
                          ['i16x8', 'i16x8', 'i16x8', 'i16x8']],
            ["bitselect", ['0xAAAA', '0x5555', ['0x5555', '0xAAAA', '0x0000', '0xFFFF']],
                          [['0x0000', '0xFFFF', '0x5555', '0xAAAA']],
                          ['i16x8', 'i16x8', 'i16x8', 'i16x8']],

            # i32x4
            ['#', 'i32x4'],
            ["not", ['0'], ['-1'], ['i32x4', 'i32x4']],
            ["not", ['-1'], ['0'], ['i32x4', 'i32x4']],
            ["not", [['-1', '0', '-1', '0']], [['0', '-1', '0', '-1']], ['i32x4', 'i32x4']],
            ["not", [['0', '-1', '0', '-1']], [['-1', '0', '-1', '0']], ['i32x4', 'i32x4']],
            ["not", ['0x55555555'], ['0xAAAAAAAA'], ['i32x4', 'i32x4']],
            ["not", ['3435973836'], ['858993459'], ['i32x4', 'i32x4']],
            ["and", [['0', '-1'], ['0', '-1', '0', '-1']], [['0', '0', '0', '-1']], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['0', '0'], ['0'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['0', '-1'], ['0'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['0', '0xFFFFFFFF'], ['0'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['1', '1'], ['1'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['255', '85'], ['85'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['255', '128'], ['128'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['2863311530', ['10', '128', '5', '165']], [['10', '128', '0', '160']],
                    ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['0xFFFFFFFF', '0x55555555'], ['0x55555555'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['0xFFFFFFFF', '0xAAAAAAAA'], ['0xAAAAAAAA'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['0xFFFFFFFF', '0x0'], ['0x0'], ['i32x4', 'i32x4', 'i32x4']],
            ["and", ['0x55555555', ['0x5555', '0xFFFF', '0x55FF', '0x5FFF']], ['0x5555'],
                    ['i32x4', 'i32x4', 'i32x4']],
            ["or", [['0', '0', '-1', '-1'], ['0', '-1', '0', '-1']], [['0', '-1', '-1', '-1']],
                   ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['0', '0'], ['0'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['0', '-1'], ['-1'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['0', '0xFFFFFFFF'], ['0xFFFFFFFF'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['1', '1'], ['1'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['255', '85'], ['255'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['255', '128'], ['255'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['2863311530', ['10', '128', '5', '165']], [['2863311530', '2863311535']],
                   ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['0xFFFFFFFF', '0x55555555'], ['0xFFFFFFFF'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['0xFFFFFFFF', '0xAAAAAAAA'], ['0xFFFFFFFF'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['0xFFFFFFFF', '0x0'], ['0xFFFFFFFF'], ['i32x4', 'i32x4', 'i32x4']],
            ["or", ['0x55555555', ['0x5555', '0xFFFF', '0x55FF', '0x5FFF']],
                   [['0x55555555', '0x5555ffff', '0x555555ff', '0x55555fff']],
                   ['i32x4', 'i32x4', 'i32x4']],
            ["xor", [['0', '0', '-1', '-1'], ['0', '-1', '0', '-1']], [['0', '-1', '-1', '0']],
                    ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['0', '0'], ['0'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['0', '-1'], ['-1'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['0', '0xFFFFFFFF'], ['0xFFFFFFFF'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['1', '1'], ['0'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['255', '85'], ['170'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['255', '128'], ['127'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['2863311530', ['10', '128', '5', '165']],
                    [['2863311520', '2863311402', '2863311535', '2863311375']],
                    ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['0xFFFFFFFF', '0x55555555'], ['0xAAAAAAAA'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['0xFFFFFFFF', '0xAAAAAAAA'], ['0x55555555'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['0xFFFFFFFF', '0x0'], ['0xFFFFFFFF'], ['i32x4', 'i32x4', 'i32x4']],
            ["xor", ['0x55555555', ['0x5555', '0xFFFF', '0x55FF', '0x5FFF']],
                    [['0x55550000', '0x5555AAAA', '0x555500AA', '0x55550AAA']],
                    ['i32x4', 'i32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0xBBBBBBBB',
                           ['0x00112345', '0xF00FFFFF', '0x10112021', '0xBBAABBAA']],
                          [['0xBBAABABA', '0xABBAAAAA', '0xABAABBBA', '0xAABBAABB']],
                          ['i32x4', 'i32x4', 'i32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0xBBBBBBBB', '0x00000000'], ['0xBBBBBBBB'],
                          ['i32x4', 'i32x4', 'i32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0xBBBBBBBB', '0x11111111'], ['0xAAAAAAAA'],
                          ['i32x4', 'i32x4', 'i32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0xBBBBBBBB',
                           ['0x01234567', '0x89ABCDEF', '0xFEDCBA98', '0x76543210']],
                          [['0xBABABABA', '0xABABABAB']],
                          ['i32x4', 'i32x4', 'i32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0x55555555',
                           ['0x01234567', '0x89ABCDEF', '0xFEDCBA98', '0x76543210']],
                          [['0x54761032', '0xDCFE98BA', '0xAB89EFCD', '0x23016745']],
                          ['i32x4', 'i32x4', 'i32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0x55555555',
                           ['0x55555555', '0xAAAAAAAA', '0x00000000', '0xFFFFFFFF']],
                          [['0x00000000', '0xFFFFFFFF', '0x55555555', '0xAAAAAAAA']],
                          ['i32x4', 'i32x4', 'i32x4', 'i32x4']],

            # f32x4
            ['#', 'f32x4'],
            ["not", ['nan'], ['0x803fffff'], ['f32x4', 'i32x4']],
            ["not", ['inf'], ['0x807fffff'], ['f32x4', 'i32x4']],
            ["not", ['-nan'], ['0x003fffff'], ['f32x4', 'i32x4']],
            ["not", ['-inf'], ['0x007fffff'], ['f32x4', 'i32x4']],
            ["not", ['0'], ['-1'], ['f32x4', 'i32x4']],
            ["not", ['-1'], ['0x407fffff'], ['f32x4', 'i32x4']],
            ["not", [['-1', '0', '-1', '0']], [['0x407fffff', '0xffffffff', '0x407fffff', '0xffffffff']], ['f32x4', 'i32x4']],
            ["not", [['0', '-1', '0', '-1']], [['0xffffffff', '0x407fffff', '0xffffffff', '0x407fffff']], ['f32x4', 'i32x4']],
            ["not", ['0x55555555'], ['0xb1555554'], ['f32x4', 'i32x4']],
            ["not", ['3435973836'], ['0xb0b33332'], ['f32x4', 'i32x4']],
            ["and", ['nan', '0'], ['0'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['nan', 'inf'], ['inf'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['nan', '0xFFFFFFFF'], ['0x4f800000'], ['f32x4', 'f32x4', 'i32x4']],
            ["and", ['inf', '0'], ['0'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['inf', 'nan'], ['inf'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['inf', '0xFFFFFFFF'], ['0x4f800000'], ['f32x4', 'f32x4', 'i32x4']],
            ["and", ['-nan', '-inf'], ['-inf'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", [['0', '-1'], ['0', '-1', '0', '-1']], [['0x00000000', '0x00000000', '0x00000000', '-1']], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['0', '0'], ['0'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['0', '-1'], ['0'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['0', '0xFFFFFFFF'], ['0'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['1', '1'], ['1'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['255', '85'], ['42.5'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['255', '128'], ['128'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['2863311530', ['10', '128', '5', '165']], [['10', '128', '2.5', '160']],
                    ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['0xFFFFFFFF', '0x55555555'], ['0x4e800000'], ['f32x4', 'f32x4', 'i32x4']],
            ["and", ['0xFFFFFFFF', '0xAAAAAAAA'], ['0x4f000000'], ['f32x4', 'f32x4', 'i32x4']],
            ["and", ['0xFFFFFFFF', '0x0'], ['0x0'], ['f32x4', 'f32x4', 'f32x4']],
            ["and", ['0x55555555', ['0x5555', '0xFFFF', '0x55FF', '0x5FFF']], [['21845', '10922.5', '21845', '21845']],
                    ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['nan', '0'], ['nan'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['nan', 'inf'], ['nan'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['nan', '0xFFFFFFFF'], ['nan'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['inf', '0'], ['inf'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['inf', 'nan'], ['nan'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['inf', '0xFFFFFFFF'], ['inf'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['-nan', '-inf'], ['-nan'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", [['0', '0', '-1', '-1'], ['0', '-1', '0', '-1']], [['0', '-1', '-1', '-1']],
                   ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['0', '0'], ['0'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['0', '-1'], ['-1'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['0', '0xFFFFFFFF'], ['0xFFFFFFFF'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['1', '1'], ['1'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['255', '85'], ['510'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['255', '128'], ['255'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['2863311530', ['10', '128', '5', '165']], [['0x4f2aaaab', '0x4f2aaaab', '0x4faaaaab', '0x4f2faaab']],
                   ['f32x4', 'f32x4', 'i32x4']],
            ["or", ['0xFFFFFFFF', '0x55555555'], ['0x4faaaaab'], ['f32x4', 'f32x4', 'i32x4']],
            ["or", ['0xFFFFFFFF', '0xAAAAAAAA'], ['0x4faaaaab'], ['f32x4', 'f32x4', 'i32x4']],
            ["or", ['0xFFFFFFFF', '0x0'], ['0xFFFFFFFF'], ['f32x4', 'f32x4', 'f32x4']],
            ["or", ['0x55555555', ['0x5555', '0xFFFF', '0x55FF', '0x5FFF']],
                   [['0x4eaaaaab', '0x4fffffab', '0x4eabfeab', '0x4ebffeab']],
                   ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['nan', '0'], ['nan'], ['f32x4', 'f32x4', 'f32x4']],
            ["xor", ['nan', 'inf'], ['0x00400000'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['nan', '0xFFFFFFFF'], ['0x30400000'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['inf', '0'], ['inf'], ['f32x4', 'f32x4', 'f32x4']],
            ["xor", ['inf', 'nan'], ['0x00400000'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['inf', '0xFFFFFFFF'], ['0x30000000'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['-nan', '-inf'], ['0x00400000'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", [['0', '0', '-1', '-1'], ['0', '-1', '0', '-1']], [['0', '-1', '-1', '0']],
                    ['f32x4', 'f32x4', 'f32x4']],
            ["xor", ['0', '0'], ['0'], ['f32x4', 'f32x4', 'f32x4']],
            ["xor", ['0', '-1'], ['-1'], ['f32x4', 'f32x4', 'f32x4']],
            ["xor", ['0', '0xFFFFFFFF'], ['0xFFFFFFFF'], ['f32x4', 'f32x4', 'f32x4']],
            ["xor", ['1', '1'], ['0'], ['f32x4', 'f32x4', 'f32x4']],
            ["xor", ['255', '85'], ['0x01d50000'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['255', '128'], ['0x007f0000'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['2863311530', ['10', '128', '5', '165']],
                    [['0x0e0aaaab', '0x0c2aaaab', '0x0f8aaaab', '0x0c0faaab']],
                    ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['0xFFFFFFFF', '0x55555555'], ['0x012aaaab'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['0xFFFFFFFF', '0xAAAAAAAA'], ['0x00aaaaab'], ['f32x4', 'f32x4', 'i32x4']],
            ["xor", ['0xFFFFFFFF', '0x0'], ['0xFFFFFFFF'], ['f32x4', 'f32x4', 'f32x4']],
            ["xor", ['0x55555555', ['0x5555', '0xFFFF', '0x55FF', '0x5FFF']],
                    [['0x080000ab', '0x09d555ab', '0x080154ab', '0x081554ab']],
                    ['f32x4', 'f32x4', 'i32x4']],
            ["bitselect", ['nan', '0', ['0x00000000']], [['0x00000000']], ['f32x4', 'f32x4', 'f32x4', 'f32x4']],
            ["bitselect", ['nan', '0', ['0xFFFFFFFF']], [['0xFFFFFFFF']], ['f32x4', 'f32x4', 'f32x4', 'f32x4']],
            ["bitselect", ['nan', '0', ['0xABABABAB']], [['0x4f000000']], ['f32x4', 'f32x4', 'f32x4', 'i32x4']],
            ["bitselect", ['nan', 'inf', ['0x7f800000']], [['nan']], ['f32x4', 'f32x4', 'f32x4', 'f32x4']],
            ["bitselect", ['nan', 'inf', ['0xFFFFFFFF']], [['inf']], ['f32x4', 'f32x4', 'f32x4', 'f32x4']],
            ["bitselect", ['nan', 'inf', ['0xABABABAB']], [['inf']], ['f32x4', 'f32x4', 'f32x4', 'f32x4']],
            ["bitselect", ['-nan', '-inf', ['0xABABABAB']], [['-inf']], ['f32x4', 'f32x4', 'f32x4', 'f32x4']],
            ["bitselect", ['0xAAAAAAAA', '0xBBBBBBBB',
                           ['0x00112345', '0xF00FFFFF', '0x10112021', '0xBBAABBAA']],
                          [['0x4f3aabbc', '0x4f2babbc', '0x4f3bbabd', '0x4f2abba8']],
                          ['f32x4', 'f32x4', 'f32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0xBBBBBBBB', '0x00000000'], ['0xBBBBBBBB'],
                          ['f32x4', 'f32x4', 'f32x4', 'f32x4']],
            ["bitselect", ['0xAAAAAAAA', '0xBBBBBBBB', '0x11111111'], ['0x4f3bbbbd'],
                          ['f32x4', 'f32x4', 'f32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0xBBBBBBBB',
                           ['0x01234567', '0x89ABCDEF', '0xFEDCBA98', '0x76543210']],
                          [['0x4f2abba8', '0x4f3ababa', '0x4f2babaf', '0x4f3bbbb8']],
                          ['f32x4', 'f32x4', 'f32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0x55555555',
                           ['0x01234567', '0x89ABCDEF', '0xFEDCBA98', '0x76543210']],
                          [['0x4f2aaaab', '0x4faaaaab', '0x4faaaaab', '0x4e2aaaab']],
                          ['f32x4', 'f32x4', 'f32x4', 'i32x4']],
            ["bitselect", ['0xAAAAAAAA', '0x55555555',
                           ['0x55555555', '0xAAAAAAAA', '0x00000000', '0xFFFFFFFF']],
                          [['0x4e2aaaab', '0x4faaaaab', '0x4eaaaaab', '0x4f2aaaab']],
                          ['f32x4', 'f32x4', 'f32x4', 'i32x4']],
        ]

    def gen_test_cases(self):
        """
        Generate test case file
        """
        with open('../simd_bitwise.wast', 'w+') as f_out:
            f_out.write(self.get_all_cases())


def gen_test_cases():
    """
    Generate test case file
    """
    bit_wise = SimdBitWise()
    bit_wise.gen_test_cases()


if __name__ == '__main__':
    gen_test_cases()