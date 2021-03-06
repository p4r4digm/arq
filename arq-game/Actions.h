#pragma once

#include "engine\Entity.h"
#include "GameData.h"
#include "engine\Vector.h"
#include "engine\PropertyMap.h"

class IAction
{
public:
   virtual ~IAction(){}
   
   virtual void execute(Entity *e, Float2 target)=0;
   virtual void end(Entity *e, Float2 target)=0;
};

namespace Actions
{
   std::unique_ptr<IAction> buildMeleeAction();
};