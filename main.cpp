
#include "src/CmdLineOptions.h"
#include "src/MicroCore.h"
#include "src/page.h"

#include "ext/crow/crow.h"
#include "ext/member_checker.h"

#include <fstream>

using boost::filesystem::path;

using namespace std;

// needed for log system of momero
namespace epee {
    unsigned int g_test_dbg_lock_sleep = 0;
}

int main(int ac, const char* av[]) {

    // get command line options
    xmreg::CmdLineOptions opts {ac, av};

    auto help_opt = opts.get_option<bool>("help");

    // if help was chosen, display help text and finish
    if (*help_opt)
    {
        return EXIT_SUCCESS;
    }

    auto port_opt           = opts.get_option<string>("port");
    auto bc_path_opt        = opts.get_option<string>("bc-path");
    auto custom_db_path_opt = opts.get_option<string>("custom-db-path");
    auto deamon_url_opt     = opts.get_option<string>("deamon-url");

    //cast port number in string to uint16
    uint16_t app_port = boost::lexical_cast<uint16_t>(*port_opt);

    // get blockchain path
    path blockchain_path;

    if (!xmreg::get_blockchain_path(bc_path_opt, blockchain_path))
    {
        cerr << "Error getting blockchain path." << endl;
        return EXIT_FAILURE;
    }

     // enable basic monero log output
    xmreg::enable_monero_log();

    // create instance of our MicroCore
    // and make pointer to the Blockchain
    xmreg::MicroCore mcore;
    cryptonote::Blockchain* core_storage;

    // initialize mcore and core_storage
    if (!xmreg::init_blockchain(blockchain_path.string(),
                               mcore, core_storage))
    {
        cerr << "Error accessing blockchain." << endl;
        return EXIT_FAILURE;
    }

    // check if we have path to lmdb2 (i.e., custom db)
    // and if it exists

    string custom_db_path_str;

    if (custom_db_path_opt)
    {
        if (boost::filesystem::exists(boost::filesystem::path(*custom_db_path_opt)))
        {
            custom_db_path_str = *custom_db_path_opt;
        }
        else
        {
            cerr << "Custom db path: " << *custom_db_path_opt
                 << "does not exist" << endl;

            return EXIT_FAILURE;
        }
    }
    else
    {
        // if not given assume it is located in ~./bitmonero/lmdb2 folder
        custom_db_path_str = blockchain_path.parent_path().string()
                             + string("/lmdb2");
    }

    custom_db_path_str = xmreg::remove_trailing_path_separator(custom_db_path_str);


    // create instance of page class which
    // contains logic for the website
    xmreg::page xmrblocks(&mcore, core_storage,
                          *deamon_url_opt, custom_db_path_str);

    // crow instance
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([&]() {
        return xmrblocks.index2();
    });


    CROW_ROUTE(app, "/myoutputs").methods("GET"_method)
    ([&](const crow::request& req) {

        string xmr_address  = string(req.url_params.get("xmr_address"));
        string viewkey      = string(req.url_params.get("viewkey"));
        uint64_t since_when = boost::lexical_cast<uint64_t>(
                                   req.url_params.get("sincewhen"));

        return xmrblocks.show_my_outputs(xmr_address, viewkey, since_when);
    });

    // run the crow http server
    app.port(app_port).multithreaded().run();

    return EXIT_SUCCESS;
}
