#!/usr/bin/env python3

"""Common integer value operations"""

from simd_lane_value import LaneValue


class ArithmeticOp:
    """This class provides methods to simulate integer arithmetic
    and saturating integer arithmetic operations for the purpose of
    getting corresponding expected results. One or more operands
    may be required for the operations.
    The following operators are covered:
    add, sub, mul, neg,
    add_saturate_s, add_saturate_u,
    sub_saturate_s, sub_saturate_u,
    min_s, min_u, max_s, max_u, avgr_u, abs
    """
    def __init__(self, op: str):
        self.op = op

    @staticmethod
    def get_valid_value(value: int, lane: LaneValue, signed=True) -> int:
        """Get the valid integer value in the scope of the specified lane size.

        For a integer value, convert it to the valid value with the same bits
        of the lane width. The value can be signed or unsigned, with the scope
        of -0x80... to 0x7f... or 0 to 0xff...

        :param value: the value of the integer
        :param lane: the LaneValue instance of a lane in v128
        :param signed: specify if the lane is interpreted as a signed or
                        an unsigned number.
        :return : the valid value in either signed or unsigned number
        """
        value &= lane.mask
        if signed:
            if value > lane.max:
                return value - lane.mod
            if value < lane.min:
                return value + lane.mod
        return value

    def saturate(self, p1: int, p2: int, lane: LaneValue) -> int:
        """Get the result of saturating arithmetic operation on 2 operands.
        The operands can be both signed or unsigned. The following ops
        are covered:
        add_saturate_s, sub_saturate_s, add_saturate_u, sub_saturate_u,

        Saturating arithmetic can make sure:
        When the operation result is less than the minimum, return the minimum.
        When the operation result is greater than the maximum, return the maximum.
        For other operation results, simply return themselves.
        :param p1: the integer operand 1
        :param p2: the integer operand 2
        :param lane: the LaneValue instance of a lane in v128
        :return: the result of the saturating arithmetic operation
        """
        if self.op.endswith('saturate_s'):
            if p1 > lane.max:
                p1 -= lane.mod
            if p2 > lane.max:
                p2 -= lane.mod

            if self.op.startswith('add'):
                value = p1 + p2
            if self.op.startswith('sub'):
                value = p1 - p2

            if value > lane.max:
                return lane.max
            if value < lane.min:
                return lane.min

        if self.op.endswith('saturate_u'):
            if p1 < 0:
                p1 += lane.mod
            if p2 < 0:
                p2 += lane.mod
            if self.op.startswith('add'):
                value = p1 + p2
            if self.op.startswith('sub'):
                value = p1 - p2

            if value > lane.mask:
                return lane.mask
            if value < 0:
                return 0

        return value

    def unary_op(self, p, lane):
        """General integer arithmetic and saturating arithmetic operations
        with only one operand.

        Supported ops: neg, abs

        :param p: the operand, integer or literal string in hex or decimal format
        :param lane: the LaneValue instance of a lane in v128
        :return: the string of the result of <self.op p> in hex or decimal format
        """
        v = p
        base = 10
        if isinstance(p, str):
            if '0x' in p:
                base = 16
            v = int(p, base)

        if self.op == 'neg':
            result = self.get_valid_value(-v, lane)
        elif self.op == 'abs':
            result = self.get_valid_value(v, lane)
            if result >= 0:
                return p
            else:
                result = -result
            if base == 16:
                return hex(result)
        else:
            raise Exception('Unknown unary operation')

        return str(result)

    def binary_op(self, p1, p2, lane):
        """General integer arithmetic and saturating arithmetic operations
        with 2 operands.

        Supported ops:
        add, sub, mul,
        add_saturate_s, add_saturate_u,
        sub_saturate_s, sub_saturate_u,
        min_s, min_u, max_s, max_u, avgr_u

        :param p1: the operand 1, integer or literal string in hex or decimal format
        :param p2: the operand 2, integer or literal string in hex or decimal format
        :param lane: the LaneValue instance of a lane in v128
        :return: the string of the result of <p1 self.op p2> in hex or decimal format
        """
        v1 = p1
        v2 = p2
        base1 = base2 = 10
        if isinstance(p1, str):
            if '0x' in p1:
                base1 = 16
            v1 = int(p1, base1)
        if isinstance(p2, str):
            if '0x' in p2:
                base2 = 16
            v2 = int(p2, base2)

        result_signed = True
        if self.op == 'add':
            value = v1 + v2
        elif self.op == 'sub':
            value = v1 - v2
        elif self.op == 'mul':
            value = v1 * v2
        elif 'saturate' in self.op:
            value = self.saturate(v1, v2, lane)
            if self.op.endswith('_u'):
                result_signed = False
        elif self.op in ['min_s', 'max_s']:
            i1 = self.get_valid_value(v1, lane)
            i2 = self.get_valid_value(v2, lane)
            if self.op == 'min_s':
                return p1 if i1 <= i2 else p2
            else:
                return p1 if i1 >= i2 else p2
        elif self.op in ['min_u', 'max_u']:
            i1 = self.get_valid_value(v1, lane, signed=False)
            i2 = self.get_valid_value(v2, lane, signed=False)
            if self.op == 'min_u':
                return p1 if i1 <= i2 else p2
            else:
                return p1 if i1 >= i2 else p2
        elif self.op == 'avgr_u':
            i1 = self.get_valid_value(v1, lane, signed=False)
            i2 = self.get_valid_value(v2, lane, signed=False)
            result = (i1 + i2 + 1) // 2
            if base1 == 16 or base2 == 16:
                return hex(result)
            else:
                return str(result)
        else:
            raise Exception('Unknown binary operation')

        result = self.get_valid_value(value, lane, signed=result_signed)
        return str(result)
