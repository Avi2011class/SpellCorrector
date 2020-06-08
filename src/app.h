#pragma once

#include <Poco/Util/ServerApplication.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/IntValidator.h>
#include <Poco/Util/OptionCallback.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Logger.h>

#include "web_server.h"

using namespace Poco::Util;


class CorrectorServerApp : public ServerApplication
{
public:
    void initialize(Application& application) override;
    void defineOptions(OptionSet& options) override;
    int main(const std::vector<std::string>& flags) override;


    void displayHelp() {
        Poco::Util::HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("OPTIONS");
        helpFormatter.setHeader("A web server that corrects typos");
        helpFormatter.format(std::cout);
    }

protected:
    void setDictionaryPath(const std::string&, const std::string& value);
    void setMetricConfigPath(const std::string&, const std::string& value);
    void setAddress(const std::string&, const std::string& value);
    void setPort(const std::string&, const std::string& value);
    void handleHelp(const std::string& name, const std::string& value);

    std::shared_ptr<AbstractWStringMetric> getMetric() const;

    bool is_help_requested_ = false;
};

int CorrectorServerApp::main(const std::vector<std::string>& flags) {
    if (is_help_requested_) {
        displayHelp();
        return ServerApplication::EXIT_OK;
    }
    auto metric = getMetric();
    auto dictionary = std::make_shared<BKTree>(this->config().getString("dictionary_path"), metric);

    auto handler_factory = new CorrectorHandlerFactory(dictionary);
    auto params = new HTTPServerParams;

    params->setMaxQueued(1000);
    params->setMaxThreads(8);
    params->setTimeout(1000);

    std::string address = this->config().getString("address", "0.0.0.0");
    int port = this->config().getInt("port", 9000);

    auto socket_server =
            std::make_shared<ServerSocket>(SocketAddress(address, port));

    HTTPServer server(handler_factory, *socket_server, params);

    server.start();
    std::wcout << std::endl << "Server started" << std::endl;

    waitForTerminationRequest();

    std::wcout << std::endl << "Shutting down..." << std::endl;
    server.stop();

    return Application::EXIT_OK;
}

void CorrectorServerApp::defineOptions(OptionSet& options) {
    ServerApplication::defineOptions(options);
    options.addOption(
            Option("help", "h", "Show additional info")
                    .repeatable(false)
                    .required(false)
                    .callback(OptionCallback<CorrectorServerApp>(this, &CorrectorServerApp::handleHelp))
    );

    options.addOption(
            Option("dictionary_path", "d", "Path to dictionary file")
                    .repeatable(true)
                    .required(true)
                    .argument("dictionary_path", true)
                    .callback(OptionCallback<CorrectorServerApp>(this, &CorrectorServerApp::setDictionaryPath))
    );

    options.addOption(
            Option("metric_config", "m", "Path to metric description file")
                    .repeatable(false)
                    .required(false)
                    .argument("metric_config", true)
                    .callback(OptionCallback<CorrectorServerApp>(this, &CorrectorServerApp::setMetricConfigPath))
    );

    options.addOption(
            Option("address", "a", "Host to serve app")
                    .repeatable(false)
                    .required(false)
                    .argument("address", true)
                    .callback(OptionCallback<CorrectorServerApp>(this, &CorrectorServerApp::setAddress))
    );

    options.addOption(
            Option("port", "p", "Port to serve app")
                    .repeatable(false)
                    .required(false)
                    .argument("port", true)
                    .validator(new Poco::Util::IntValidator(1, 65536))
                    .callback(OptionCallback<CorrectorServerApp>(this, &CorrectorServerApp::setPort))
    );
}

void CorrectorServerApp::setMetricConfigPath(const std::string&, const std::string& value) {
    this->config().setString("metric_config", value);
}

void CorrectorServerApp::setDictionaryPath(const std::string&, const std::string& value) {
    this->config().setString("dictionary_path", value);
}

void CorrectorServerApp::initialize(Application& application) {
    setlocale(LC_ALL, "");
    ServerApplication::initialize(application);
}

std::shared_ptr<AbstractWStringMetric> CorrectorServerApp::getMetric() const {
    if (!this->config().hasProperty("metric_config")) {
        std::cerr << "Default Levenstein metric will be used" << std::endl;
        return std::make_shared<LevensteinMetric>();
    } else {
        std::string metric_config_name = this->config().getString("metric_config");
        try {
            std::cerr << Poco::format("Parsing metric config file: %s...", metric_config_name);
            auto result = std::make_shared<WeightedLevensteinMetric>(metric_config_name);
            std::cerr << "Done!" << std::endl;
            return result;
        } catch (std::exception& e) {
            std::cerr << "Failed!" << std::endl;
            std::ostringstream log;
            log << Poco::format("Error creating weighted lewenstein metric from file \"%s\" ", metric_config_name);
            log << std::endl << e.what() << std::endl;
            throw std::runtime_error(log.str());
        }
    }
}

void CorrectorServerApp::handleHelp(const std::string& name, const std::string& value) {
    if (name == "help") {
        is_help_requested_ = true;
        stopOptionsProcessing();
    }
}

void CorrectorServerApp::setAddress(const std::string&, const std::string& value) {
    this->config().setString("address", value);
}

void CorrectorServerApp::setPort(const std::string&, const std::string& value) {
    this->config().setInt("port", std::stoi(value));
}
