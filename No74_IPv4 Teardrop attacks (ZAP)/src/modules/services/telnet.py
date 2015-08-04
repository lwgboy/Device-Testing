import util
import socket
from colors import color
from service import Service
from threading import Thread
from zoption import Zoption


class telnet(Service):
    def __init__(self):
        super(telnet, self).__init__('telnet server')
        self.server_thread = None
        self.server_socket = None
        self.config['port'].value = 23
        self.config.update({"server":Zoption(type = "str",
                                      value = "Unified",
                                      required = False,
                                      display = "Server title to spoof")
                           })
        self.info = """
                    Simple telnet emulator; just grabs a username/password
                    and denies access.  Could be extended to be a sort of
                    honeypot system.
                    """

    def response(self, con, msg):
        """ Respond to connection
        """
        con.send('%s' % msg)

    def initialize_bg(self):
        """ initialize background service
        """
        util.Msg('Starting telnet service...')
        self.server_thread = Thread(target=self.initialize)
        self.server_thread.start()
        return True

    def initialize(self):
        """ initialize; blocking
        """
        self.running = True
        self.server_sock = socket.socket()
        self.server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            self.server_sock.bind(('', self.config['port'].value))
        except:
            util.Error('Cannot bind to address.')
            return

        self.server_sock.settimeout(3)
        self.server_sock.listen(1)
        try:
            while self.running:
                try:
                    con, addr = self.server_sock.accept()
                except KeyboardInterrupt:
                    return
                except:
                    continue

                self.log_msg('Connection from %s' % str(addr))
                con.recv(256)  # junk

                while self.running:
                    try:
                        # username/password prompt
                        self.response(con, '%s Username: ' % 
                                                self.config['server'].value)
                        username = con.recv(256).strip().replace('\n', '')
                        if len(username) < 1:
                            continue

                        self.response(con, '%s Password: ' % 
                                                self.config['server'].value)
                        password = con.recv(256).strip().replace('\n', '')
                        if len(password) < 1:
                            continue

                        self.response(con, 'Invalid Credentials\r\n')
                        self.log_msg('Received %s%s:%s%s from connection.' %
                                (color.GREEN, username, password, color.YELLOW))
                        break
                    except socket.error:
                        break
                con.close()
                self.log_msg('%s disconnected.\n' % str(addr))
        except:
            self.server_sock.close()

    def cli(self, parser):
        """ initialize CLI options
        """
        parser.add_argument('--telnet', help='Telnet server',
            action='store_true', default=False, dest=self.which)
