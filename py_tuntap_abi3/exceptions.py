class PyWinTunException(Exception):
    pass


class PytunError(OSError):
    """Cross-platform pytun exception alias for native backend errors."""
