#include "PhysicsManager.h"
#include "engine\Component.h"
#include "ArqComponents.h"
#include "engine\IOCContainer.h"
#include "engine\Application.h"
#include "engine\CoreComponents.h"
#include "CharacterManager.h"

REGISTER_COMPONENT(VelocityComponent);

class PhysicsManagerImpl : public Manager<PhysicsManagerImpl, PhysicsManager>
{
public:
   static void registerComponentCallbacks(Manager<PhysicsManagerImpl, PhysicsManager> &m)
   {
   }

   void update()
   {
      float dt = IOC.resolve<Application>()->dt();

      for(auto &comp : m_system->getComponentVector<VelocityComponent>())
      {
         //comp.velocity.x = std::min(std::max(comp.velocity.x, -comp.maxVelocity.x), comp.maxVelocity.x);
         //comp.velocity.y = std::min(std::max(comp.velocity.y, -comp.maxVelocity.y), comp.maxVelocity.y);

         if(auto pc = comp.parent->lock<PositionComponent>())
         {
            float remainingTime = 1.0f;
            
            if (auto rem = comp.parent->get<TRemainingTimeComponent>())
               remainingTime = rem->remainingTime;

            pc->pos.x += comp.velocity.x * remainingTime * dt;
            pc->pos.y += comp.velocity.y * remainingTime * dt;
         }
         
      }


   }
};

std::unique_ptr<PhysicsManager> buildPhysicsManager()
{
   return std::unique_ptr<PhysicsManager>(new PhysicsManagerImpl());
}