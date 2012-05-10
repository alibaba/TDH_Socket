#!/usr/bin/python
#Copyright(C) 2011-2012 Alibaba Group Holding Limited
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Authors:
#   wentong <wentong@taobao.com>
# 11-1-21
#

import logging

class logger:
    logger = None

    def __init__(self, handler=None, level=logging.ERROR):
        import logging.handlers

        self.logger = logging.getLogger()
        if handler is None:
            handler = logging.StreamHandler()
            fmt = logging.Formatter(logging.BASIC_FORMAT)
            handler.setFormatter(fmt)
        self.logger.addHandler(handler)
        self.logger.setLevel(level)

    def set_level(self, level):
        self.logger.setLevel(level)

    def info(self, msg):
        self.logger.info(msg)

    def debug(self, msg):
        self.logger.debug(msg)