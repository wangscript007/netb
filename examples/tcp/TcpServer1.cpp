/*
 * Copyright (C) 2013, Maoxu Li. Email: maoxu@lebula.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Socket.hpp"

using namespace netb;


// TCP echo server
int main(const int argc, char* argv[])
{
    // Service port, default 9000
    // Given port: tcps 9001
    unsigned short port = 9000;
    if(argc == 2)
    {
        int n = atoi(argv[1]);
        if(n > 0 && n <= 65535)
        {
            port = (unsigned short)n;
        }
    }

    // TCP echo server
    // Error handling with exceptions
    try
    {
        Socket tcps(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        tcps.ReuseAddress(true);
        tcps.ReusePort(true);
        tcps.Bind(SocketAddress(port, AF_INET));
        tcps.Listen();
        std::cout << "Opened on: " << tcps.Address().String() << std::endl;

        Socket conn;
        char* buf = new char[2048];
        while(conn.Attach(tcps.Accept()))
        {
            ssize_t ret;
            while((ret = conn.Receive(buf, 2048)) > 0)
            {
                if(conn.Send(buf, ret) > 0)
                {
                    std::cout << "Receive and send back " << ret << " bytes" << std::endl;
                }
            }
        }
        delete [] buf;
    }
    catch(const Exception& ex)
    {
        std::cout << "Exception: " << ex.Report() << std::endl;
    }
    return 0;
}
