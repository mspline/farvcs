#ifndef __ENFORCE_H
#define __ENFORCE_H

//========================================================================>>
// The Enforcer is taken almost verbatim from the A.Alexadrescu's article
// http://www.ddj.com/dept/cpp/184403864.
// I reduced the macro's name to ENF though, made it non-copyable, and
// made locus_ member string, not char *
//========================================================================>>

#include <strstream>

template<typename Ref, typename P, typename R>
class Enforcer
{
public:
    Enforcer(Ref t, const std::string& sLocus) : t_(t), locus_(P::Wrong(t) ? sLocus : "")
    {
    }
    Ref operator*() const
    {
        if (!locus_.empty()) R::Throw(t_, msg_, locus_.c_str());
        return t_;
    }
    template <class MsgType>
    Enforcer& operator()(const MsgType& msg)
    {
        if (!locus_.empty()) 
        {
            // Here we have time; no need to be super-efficient
            std::ostrstream ss;
            ss << msg << std::ends;
            msg_ += ss.str();
        }
        return *this;
    }
private:
    Ref t_;
    std::string msg_;
    std::string locus_;

    Enforcer& operator=( const Enforcer& );
};
template <class P, class R, typename T>
inline Enforcer<const T&, P, R> 
MakeEnforcer(const T& t, const char* locus)
{
    return Enforcer<const T&, P, R>(t, locus);
}
template <class P, class R, typename T>
inline Enforcer<T&, P, R> 
MakeEnforcer(T& t, const char* locus)
{
    return Enforcer<T&, P, R>(t, locus);
}

struct DefaultPredicate
{
    template <class T> 
    static bool Wrong(const T& obj)
    {
        return !obj;
    }
};

struct DefaultRaiser
{
    template <class T>
    static void Throw(const T&, const std::string& message, const char* locus)
    {
        throw std::runtime_error(message + '\n' + locus);
    }
};

#define STRINGIZE(something) STRINGIZE_HELPER(something) 
#define STRINGIZE_HELPER(something) #something

#define ENF(exp) \
    *MakeEnforcer<DefaultPredicate, DefaultRaiser>( \
        (exp), "Expression '" #exp "' failed in '"  \
        __FILE__ "', line: " STRINGIZE(__LINE__))

//========================================================================>>
// My custom Predicate macros
//========================================================================>>

struct HandlePredicate
{
    static bool Wrong( HANDLE handle )
    {
        return handle == INVALID_HANDLE_VALUE;
    }
};

#define ENF_H(exp) \
    *MakeEnforcer<HandlePredicate, DefaultRaiser>( (exp), "Expression '" #exp "' failed in '" __FILE__ "', line: " STRINGIZE(__LINE__) )

#endif // __ENFORCE_H
