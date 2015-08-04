# Copyright (C) 2012  Aldo Cortesi
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os, traceback

class ScriptError(Exception):
    pass


class Script:
    """
        The instantiator should do something along this vein:

            s = Script(path, master)
            s.load()
            s.run("start")
    """
    def __init__(self, path, ctx):
        self.path, self.ctx = path, ctx
        self.ns = None

    def load(self):
        """
            Loads a module.

            Raises ScriptError on failure, with argument equal to an error
            message that may be a formatted traceback.
        """
        path = os.path.expanduser(self.path)
        if not os.path.exists(path):
            raise ScriptError("No such file: %s"%self.path)
        if not os.path.isfile(path):
            raise ScriptError("Not a file: %s"%self.path)
        ns = {}
        try:
            execfile(path, ns, ns)
        except Exception, v:
            raise ScriptError(traceback.format_exc(v))
        self.ns = ns

    def run(self, name, *args, **kwargs):
        """
            Runs a plugin method.

            Returns:

                (True, retval) on success.
                (False, None) on nonexistent method.
                (False, (exc, traceback string)) if there was an exception.
        """
        f = self.ns.get(name)
        if f:
            try:
                return (True, f(self.ctx, *args, **kwargs))
            except Exception, v:
                return (False, (v, traceback.format_exc(v)))
        else:
            return (False, None)
