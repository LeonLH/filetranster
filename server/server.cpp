#include <iostream>
#include <file_transfer.h>

int main(int argc, char* argv[])
{
    ft::TransferPtr tran;
    try {

        if (argc >= 3)
        {
            tran = ft::make_transfer_server(argv[1], argv[2]);
        }
        else
        {
            tran = ft::make_transfer_server("0.0.0.0", "6666");
        }
    }
    catch (const std::exception& e)
    {
        std::cout << e.what();
    }

    std::cin.ignore();

    return 0;
}