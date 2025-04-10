#pragma once

#include "light.h"

class RectLight : public ILight
{
public:
    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
};