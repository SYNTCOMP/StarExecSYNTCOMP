#!/usr/bin/env python3
"""
Regrouping helper functions

Philipp Schlehuber-Caissier @ EPITA, 2023
"""

import math

def getScore(syntTotal, syntRef):
    assert syntTotal >= syntRef
    return max(0.0, 2.0 - math.log((syntTotal + 1.0) / (syntRef + 1.0), 10))