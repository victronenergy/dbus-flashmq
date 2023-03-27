#!/usr/bin/python3 -u
# -*- coding: utf-8 -*-

import os
import sys

AppDir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(1, os.path.join(AppDir, 'ext', 'velib_python'))

from ext.velib_python.mosquitto_bridge_registrator import MosquittoBridgeRegistrator
from ext.velib_python.ve_utils import get_vrm_portal_id

vrmid = get_vrm_portal_id()

registrator = MosquittoBridgeRegistrator(vrmid)
registrator.register()
