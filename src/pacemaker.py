from threading import Event, Condition, Lock
from enum import Enum


class PacemakerEventStatus(Enum):
    NOT_SET = 1,
    TIMED_OUT = 2,
    SET = 3


class PacemakerEvent:
    """Class implementing event objects.

    Events manage a flag that can be set to true with the set() method and reset
    to false with the clear() method. The wait() method blocks until the flag is
    true.  The flag is initially false.

    """
    def __init__(self):
        self._cond = Condition(Lock())
        self._status : PacemakerEventStatus = PacemakerEventStatus.NOT_SET

    def __repr__(self):
        cls = self.__class__
        if self._status == PacemakerEventStatus.NOT_SET:
            status = 'not_set'
        elif self._status == PacemakerEventStatus.TIMED_OUT:
            status = 'timed_out'
        else:
            status = 'set'
        
        return f"<{cls.__module__}.{cls.__qualname__} at {id(self):#x}: {status}>"

    def _at_fork_reinit(self):
        # Private method called by Thread._reset_internal_locks()
        self._cond._at_fork_reinit()

    def status(self):
        """Return true if and only if the internal flag is true."""
        return self._status

    def set(self, status : PacemakerEventStatus = PacemakerEventStatus.SET):
        """Set the internal flag to true.

        All threads waiting for it to become true are awakened. Threads
        that call wait() once the flag is true will not block at all.

        """
        with self._cond:
            self._status = status
            self._cond.notify_all()

    def clear(self):
        """Reset the internal flag to false.

        Subsequently, threads calling wait() will block until set() is called to
        set the internal flag to true again.

        """
        with self._cond:
            self._status = PacemakerEventStatus.NOT_SET

    def wait(self, status = PacemakerEventStatus.SET, timeout=None):
        """Block until the internal flag is true.

        If the internal flag is true on entry, return immediately. Otherwise,
        block until another thread calls set() to set the flag to true, or until
        the optional timeout occurs.

        When the timeout argument is present and not None, it should be a
        floating point number specifying a timeout for the operation in seconds
        (or fractions thereof).

        This method returns the internal flag on exit, so it will always return
        True except if a timeout is given and the operation times out.

        """
        with self._cond:
            if not (self._status == status): 
                if timeout:
                    if not self._cond.wait(timeout):
                        self._status = PacemakerEventStatus.TIMED_OUT
                else:
                    while self._cond.wait():
                        if self._status == status:
                            break


class Pacemaker():
    def __init__(self):
        self.new_view_event = PacemakerEvent()
        self.central_control_event = PacemakerEvent()
