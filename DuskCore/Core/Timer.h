/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class Timer
{
public:
               Timer();

    void    start();
    void    reset();

    f64     getDelta();
    f64     getDeltaAsMiliseconds();
    f64     getDeltaAsSeconds();

    f64     getElapsedTime() const;
    f64     getElapsedTimeAsMiliseconds() const;
    f64     getElapsedTimeAsSeconds() const;

private:
    i64     startTime;
    i64     previousTime;
};
