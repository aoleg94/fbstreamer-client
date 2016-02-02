#ifndef NET_H
#define NET_H

int do_connect(const char* ip);
int poll_frame();
int get_jpeg_data(void** data);

#endif // NET_H

