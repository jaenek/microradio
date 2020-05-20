#!/usr/bin/env python
import io
import os
import random
import shutil
import socketserver
import subprocess

from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = 8000

def random_song():
    """Pick a random file with .flac or .mp3 extension from current path."""
    songs = []
    for dirpath, dirnames, filenames in os.walk('.'):
        for filename in [f for f in filenames if f.endswith(('.flac', '.mp3'))]:
            songs.append(os.path.join(dirpath, filename))

    return random.choice(songs)


class FileStreamingHandler(BaseHTTPRequestHandler):
    """Handles GET requests:
        - '/*' - streams random reencoded music file.
    """
    def setup(self):
        socketserver.StreamRequestHandler.setup(self)
        # wrap the wfile with a class that will eat up "Broken pipe" errors
        self.wfile = _SocketWriter(self.wfile)

    def reencode_audio(self, filename):
        """Reencode music files with lower bitrate."""
        args = [
            'sox',
            filename,
            '-C', '128',
            '-t', 'mp3',
            '-'
        ]

        self.log_message('"SOX {}"'.format(filename))

        ph = subprocess.Popen(
            args, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )

        out, err = ph.communicate()
        status = ph.returncode

        if status == 0:
            return out

        self.log_message('"SOX" {}'.format(err.decode('utf-8')))
        return None

    def do_GET(self):
        encoded = self.reencode_audio(random_song())
        if encoded is None:
            self.send_response(404)
            return

        f = io.BytesIO()
        f.write(encoded)
        f.seek(0)

        self.send_response(200)
        self.send_header('Content-type', 'audio/mpeg')
        self.send_header('Content-length', str(len(encoded)))
        self.end_headers()

        if f:
            try:
                shutil.copyfileobj(f, self.wfile)
            except ClientAbortedException:
                self.log_message('"CLOSED %s"', self.path)
            finally:
                f.close()

    def finish(self):
        # if the other end breaks the connection, these operations will fail
        try:
            self.wfile.close()
        except socket.error:
            pass
        try:
            self.rfile.close()
        except socket.error:
            pass

class _SocketWriter:
    """This class ignores 'Broken pipe' errors."""
    def __init__(self, wfile):
        self.wfile = wfile

    def __getattr__(self, name):
        return getattr(self.wfile, name)

    def write(self, buf):
        try:
            return self.wfile.write(buf)
        except IOError as v:
            if v.errno == 32 or v.errno == 104:
                raise ClientAbortedException
            else:
                # not a 'Broken pipe' or Connection reset by peer
                # re-raise the error
                raise

class ClientAbortedException(Exception):
    pass

if __name__ == '__main__':
    httpd = HTTPServer(('', PORT), FileStreamingHandler)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass

    httpd.server_close()
    print('Server stopped')
