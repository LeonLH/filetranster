// client.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <windows.h>
#include <iostream>
#include <file_transfer.h>

int main(int argc, char* argv[])
{
    ft::TransferPtr tran;
    try {
        if (argc >= 3)
        {
            tran = ft::make_transfer_client(argv[1], argv[2]);
        }
        else {
            tran = ft::make_transfer_client("127.0.0.1", "6666", "./rcv/");
        }

        //ft::send(tran, "foo", "xiaomi", "ts", "20200606", "930", "1530");

        //std::cout << "query\n";
        //::Sleep(3000);

        ft::query(tran, "xiaomi", "ts", "20200606", "930", "1530");
    }
    catch (const std::exception& e)
    {
        std::cout << e.what();
    }

    std::cin.ignore();

    ft::stop(tran);

    return 0;
}

