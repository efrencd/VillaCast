import socket
host,port='127.0.0.1',18891
for method,path,status in [('GET','/',b'200 OK'),('HEAD','/',b'200 OK'),('GET','/unknown',b'404 Not Found'),('POST','/',b'404 Not Found')]:
    with socket.create_connection((host,port),timeout=3) as s:
        s.sendall(f'{method} {path} HTTP/1.1\r\nHost: localhost\r\n\r\n'.encode())
        response=b''
        while True:
            chunk=s.recv(65536)
            if not chunk:break
            response+=chunk
    assert status in response.split(b'\r\n',1)[0],response[:100]
    head,body=response.split(b'\r\n\r\n',1)
    assert b'Cache-Control: no-store' in head
    if method=='HEAD':assert body==b''
    elif status==b'200 OK':
        assert b'__WEBRTC_PORT__' not in body and b'__STREAM_PATH__' not in body and b'__LANG__' not in body
        assert b'<html lang="es">' in body or b'<html lang="en">' in body
        assert b'18889' in body and b'/mystream/whep' in body
print('PASS: viewer HTTP GET/HEAD, path/method rejection, no-cache, endpoint substitution')
