#include "expressionfunctions.h"
#include "threading.h"
#include "exprfunc.h"
#include "module.h"

std::unordered_map<String, ExpressionFunctions::Function> ExpressionFunctions::mFunctions;

//Copied from http://stackoverflow.com/a/7858971/1806760
template<int...>
struct seq {};

template<int N, int... S>
struct gens : gens < N - 1, N - 1, S... > {};

template<int... S>
struct gens<0, S...>
{
    typedef seq<S...> type;
};

template<typename T, int ...S, typename... Ts>
static T callFunc(const T* argv, T(*cbFunction)(Ts...), seq<S...>)
{
    return cbFunction(argv[S]...);
}

template<typename... Ts>
static bool RegisterEasy(const String & name, duint(*cbFunction)(Ts...))
{
    auto aliases = StringUtils::Split(name, '\1');
    if(!ExpressionFunctions::Register(aliases[0], sizeof...(Ts), [cbFunction](int argc, duint * argv, void* userdata)
{
    return callFunc(argv, cbFunction, typename gens<sizeof...(Ts)>::type());
    }))
    return false;
    for(size_t i = 1; i < aliases.size(); i++)
        ExpressionFunctions::RegisterAlias(aliases[0], aliases[i]);
    return true;
}

void ExpressionFunctions::Init()
{
    //TODO: register more functions
    using namespace Exprfunc;

    //undocumented
    RegisterEasy("src.line", srcline);
    RegisterEasy("src.disp", srcdisp);

    RegisterEasy("mod.party", modparty);
    RegisterEasy("mod.base", ModBaseFromAddr);
    RegisterEasy("mod.size", ModSizeFromAddr);
    RegisterEasy("mod.hash", ModHashFromAddr);
    RegisterEasy("mod.entry", ModEntryFromAddr);

    RegisterEasy("disasm.sel\1dis.sel", disasmsel);
    RegisterEasy("dump.sel", dumpsel);
    RegisterEasy("stack.sel", stacksel);

    RegisterEasy("peb\1PEB", peb);
    RegisterEasy("teb\1TEB", teb);
    RegisterEasy("tid\1TID\1ThreadId", tid);

    RegisterEasy("bswap", bswap);
}

bool ExpressionFunctions::Register(const String & name, int argc, CBEXPRESSIONFUNCTION cbFunction, void* userdata)
{
    if(!isValidName(name))
        return false;
    EXCLUSIVE_ACQUIRE(LockExpressionFunctions);
    if(mFunctions.count(name))
        return false;
    Function f;
    f.name = name;
    f.argc = argc;
    f.cbFunction = cbFunction;
    f.userdata = userdata;
    mFunctions[name] = f;
    return true;
}

bool ExpressionFunctions::RegisterAlias(const String & name, const String & alias)
{
    EXCLUSIVE_ACQUIRE(LockExpressionFunctions);
    auto found = mFunctions.find(name);
    if(found == mFunctions.end())
        return false;
    if(!Register(alias, found->second.argc, found->second.cbFunction, found->second.userdata))
        return false;
    found->second.aliases.push_back(alias);
    return true;
}

bool ExpressionFunctions::Unregister(const String & name)
{
    EXCLUSIVE_ACQUIRE(LockExpressionFunctions);
    auto found = mFunctions.find(name);
    if(found == mFunctions.end())
        return false;
    auto aliases = found->second.aliases;
    mFunctions.erase(found);
    for(const auto & alias : found->second.aliases)
        Unregister(alias);
    return true;
}

bool ExpressionFunctions::Call(const String & name, std::vector<duint> & argv, duint & result)
{
    SHARED_ACQUIRE(LockExpressionFunctions);
    auto found = mFunctions.find(name);
    if(found == mFunctions.end())
        return false;
    const auto & f = found->second;
    if(f.argc != int(argv.size()))
        return false;
    result = f.cbFunction(f.argc, argv.data(), f.userdata);
    return true;
}

bool ExpressionFunctions::GetArgc(const String & name, int & argc)
{
    SHARED_ACQUIRE(LockExpressionFunctions);
    auto found = mFunctions.find(name);
    if(found == mFunctions.end())
        return false;
    argc = found->second.argc;
    return true;
}

bool ExpressionFunctions::isValidName(const String & name)
{
    if(!name.length())
        return false;
    if(!(name[0] == '_' || isalpha(name[0])))
        return false;
    for(const auto & ch : name)
        if(!(isalnum(ch) || ch == '_' || ch == '.'))
            return false;
    return true;
}
