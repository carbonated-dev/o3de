#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import fnmatch
import os
from typing import List

DEFAULT_ALLOWEDLIST_FILE = os.path.join(os.path.dirname(__file__), 'pal_allowedlist.txt')
"""The path to the default allowed-list file"""

""" CARBONATED begin """
CARBONATED_ALLOWEDLIST_FILE = os.path.join(os.path.dirname(__file__), 'carbonated_pal_allowedlist.txt')
"""The path to the optional carbonated allowed-list file"""
""" CARBONATED end """

class PALAllowedlist:
    """A utility class used for determining if PAL rules should apply to a particular file path. If a path matches the
    allowed-list, then PAL rules should not apply to that path."""

    def __init__(self, patterns: List[str]) -> None:
        """Creates a new instance of :class:`PALAllowedlist` from a list of glob patterns

        :param patterns: a list of glob patterns
        """
        self.patterns: List[str] = patterns

    def is_match(self, path: str) -> bool:
        """Determines if a path matches the allowed-list

        :param path: the path to match
        :return: True if the path matches the allowed-list, and False otherwise
        """
        for pattern in self.patterns:
            if fnmatch.fnmatch(path, pattern):
                return True
        return False

def load() -> PALAllowedlist:
    """ CARBONATED begin """
    """Returns an instance of :class:`PALAllowedlist` created from the glob patterns in
    :const:`DEFAULT_ALLOWEDLIST_FILE` and optionally from :const:`CARBONATED_ALLOWEDLIST_FILE`"""
    patterns = []

    if os.path.exists(DEFAULT_ALLOWEDLIST_FILE):
        with open(DEFAULT_ALLOWEDLIST_FILE) as fh:
            patterns.extend(fh.read().splitlines())

    if os.path.exists(CARBONATED_ALLOWEDLIST_FILE):
        with open(CARBONATED_ALLOWEDLIST_FILE) as fh:
            patterns.extend(fh.read().splitlines())

    return PALAllowedlist(patterns)
    """ CARBONATED end """
