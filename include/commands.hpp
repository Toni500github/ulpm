#include <string>
#include <vector>

class PmBase {
public:
    virtual ~PmBase() = default;

    const std::string& get_pm_command() const { return main_cmd; }

    virtual const std::vector<std::string>& get_install_cmd() const { return m_install_cmd; }
    virtual const std::vector<std::string>& get_remove_cmd()  const { return m_remove_cmd; }
    virtual const std::vector<std::string>& get_update_cmd()  const { return m_update_cmd; }
    virtual const std::vector<std::string>& get_init_cmd()    const { return m_init_cmd; }

protected:
    explicit PmBase(std::string cmd,
                    std::string install = "install",
                    std::string remove  = "remove",
                    std::string update  = "update",
                    std::string init    = "init")
        : main_cmd(std::move(cmd)),
          m_install_cmd{main_cmd, std::move(install)},
          m_remove_cmd{main_cmd, std::move(remove)},
          m_update_cmd{main_cmd, std::move(update)},
          m_init_cmd{main_cmd, std::move(init)}
    {}

private:
    std::string              main_cmd;
    std::vector<std::string> m_install_cmd;
    std::vector<std::string> m_remove_cmd;
    std::vector<std::string> m_update_cmd;
    std::vector<std::string> m_init_cmd;
};

class Npm : public PmBase {
public:
    Npm() : PmBase("npm") {}
};

class Yarn : public PmBase {
public:
    // Yarn uses `add` instead of `install`
    Yarn() : PmBase("yarn", "add") {}
};
