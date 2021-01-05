#!/usr/bin/env python

from socketIO_client import SocketIO

from vtr_interface import SOCKET_ADDRESS, SOCKET_PORT
from vtr_mission_planning.mission_client import MissionClient

import logging
log = logging.getLogger('SocketClient')
log.setLevel(logging.INFO)


class SocketMissionClient(MissionClient):
  """Subclass of a normal mission client that caches robot/path data and pushes
  notifications out over Socket.io
  """

  def __init__(self, group=None, name=None, args=(), kwargs={}):
    # super().__init__(group=group, name=name, args=args, kwargs=kwargs)
    super().__init__()

    self._socketio = None
    self._send = lambda x: log.info(
        "Dropping message because socket client isn't ready: %s", str(x))

  def kill_server(self):
    """Kill the socket server because it doesn't die automatically"""
    log.info('Killing the SocketIO server.')
    self._socketio.emit('kill')

  def _after_start_hook(self):
    """Launch the socket client post-startup"""
    self._socketio = SocketIO(SOCKET_ADDRESS,
                              SOCKET_PORT,
                              wait_for_connection=True)
    self._send = self._socketio.send

  def _after_listen_hook(self, func, args, kwargs):
    self._send({'type': func.name, 'args': args, 'kwargs': kwargs})