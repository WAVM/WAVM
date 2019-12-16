#!/usr/bin/env python3

"""Common integer value operations"""

from simd_lane_value import LaneValue

class IntegerSimpleOp:
    """Common integer simple ops:
        min_s, min_u, max_s, max_u
    """

    @staticmethod
    def binary_op(op: str, p1: str, p2: str, lane_width: int) -> str:
        """Binary operation on p1 and p2 with the operation specified by op

        :param op: min_s, min_u, max_s, max_u
        :param p1: integer value in hex
        :param p2: integer value in hex
        :lane_width: bit number of each lane in SIMD v128
        :return:
        """
        inputs = {}

        if '0x' in p2:
            i2 = int(p2, 0)
            inputs[i2] = p2
        else:
            i2 = int(p2)
            inputs[i2] = p2

        if '0x' in p1:
            i1 = int(p1, 0)
            inputs[i1] = p1
        else:
            i1 = int(p1)
            inputs[i1] = p1

        if op in ['min_s', 'max_s']:
            if op == 'min_s':
                result = inputs[min(i1, i2)]
            else:
                result = inputs[max(i1, i2)]

        elif op in ['min_u', 'max_u']:
            if i2 < 0:
                i2_tmp = i2 & LaneValue(lane_width).mask
                inputs[i2_tmp] = inputs[i2]
                i2 = i2_tmp

            if i1 < 0:
                i1_tmp = i1 & LaneValue(lane_width).mask
                inputs[i1_tmp] = inputs[i1]
                i1 = i1_tmp

            if op == 'min_u':
                result = inputs[min(i1, i2)]
            else:
                result = inputs[max(i1, i2)]

        else:
            raise Exception('Unknown binary operation')

        return result