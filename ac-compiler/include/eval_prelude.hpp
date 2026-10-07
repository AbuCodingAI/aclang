// eval(text) for every backend: the arithmetic evaluator below is ordinary AC, compiled into the program
// whenever the source calls eval on a string (see injectEvalPrelude in main.cpp). It reads + - * / with
// parentheses, unary sign, and decimal numbers. Trailing text or a stray character aborts (SIGABRT),
// where PY would raise a SyntaxError.
#pragma once
static const char* kEvalPrelude = R"AC(
Make ac_evp_digit func(c)
    IF c is $0$
        return 0
    IF c is $1$
        return 1
    IF c is $2$
        return 2
    IF c is $3$
        return 3
    IF c is $4$
        return 4
    IF c is $5$
        return 5
    IF c is $6$
        return 6
    IF c is $7$
        return 7
    IF c is $8$
        return 8
    IF c is $9$
        return 9
    return -1

Make ac_evp_skip func(s, st)
    run = 1
    WHILST run is 1
        IF st[1] > length(s)
            run = 0
        OTHER
            IF s[st[1]] is $ $
                st[1] += 1
            OTHER
                run = 0

Make ac_evp_factor func(s, st)
    ac_evp_skip(s, st)
    IF st[1] > length(s)
        return 0.0
    c = s[st[1]]
    IF c is $($
        st[1] += 1
        v = ac_evp_expr(s, st)
        ac_evp_skip(s, st)
        IF st[1] #> length(s)
            st[1] += 1
        return v
    IF c is $-$
        st[1] += 1
        return 0.0 - ac_evp_factor(s, st)
    IF c is $+$
        st[1] += 1
        return ac_evp_factor(s, st)
    whole = 0.0
    d = ac_evp_digit(c)
    IF d < 0
        IF c is $.$
            whole = 0.0
        OTHER
            /kill
    run = 1
    WHILST run is 1
        IF st[1] > length(s)
            run = 0
        OTHER
            d = ac_evp_digit(s[st[1]])
            IF d < 0
                run = 0
            OTHER
                whole = whole * 10.0 + to_dec(d)
                st[1] += 1
    IF st[1] #> length(s)
        IF s[st[1]] is $.$
            st[1] += 1
            scale = 1.0
            run = 1
            WHILST run is 1
                IF st[1] > length(s)
                    run = 0
                OTHER
                    d = ac_evp_digit(s[st[1]])
                    IF d < 0
                        run = 0
                    OTHER
                        scale = scale / 10.0
                        whole = whole + to_dec(d) * scale
                        st[1] += 1
    return whole

Make ac_evp_term func(s, st)
    v = ac_evp_factor(s, st)
    run = 1
    WHILST run is 1
        ac_evp_skip(s, st)
        IF st[1] > length(s)
            run = 0
        OTHER
            c = s[st[1]]
            IF c is $*$
                st[1] += 1
                v = v * ac_evp_factor(s, st)
            OTHER
                IF c is $/$
                    st[1] += 1
                    d = ac_evp_factor(s, st)
                    IF d is 0.0
                        v = 0.0
                    OTHER
                        v = v / d
                OTHER
                    run = 0
    return v

Make ac_evp_expr func(s, st)
    v = ac_evp_term(s, st)
    run = 1
    WHILST run is 1
        ac_evp_skip(s, st)
        IF st[1] > length(s)
            run = 0
        OTHER
            c = s[st[1]]
            IF c is $+$
                st[1] += 1
                v = v + ac_evp_term(s, st)
            OTHER
                IF c is $-$
                    st[1] += 1
                    v = v - ac_evp_term(s, st)
                OTHER
                    run = 0
    return v

Make ac_eval_str func(s)
    st = [1]
    v = ac_evp_expr(s, st)
    ac_evp_skip(s, st)
    IF st[1] > length(s)
        return v
    /kill
    return v

)AC";
