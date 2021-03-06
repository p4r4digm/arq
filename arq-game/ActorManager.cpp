#include "engine\Component.h"
#include "engine\StateMachine.h"
#include "engine\CoreComponents.h"
#include "engine\ComponentHelpers.h"
#include "engine\GameMath.h"
#include "engine\IOCContainer.h"
#include "engine\Application.h"

#include "ActorManager.h"
#include "GridManager.h"
#include "ArqComponents.h"
#include "PartitionManager.h"
#include "GameData.h"
#include "engine\Logs.h"

#include "boost\optional.hpp"

REGISTER_COMPONENT(ActorComponent);

class ActorState
{
protected:
   EntitySystem *m_system;
   Entity *e;
   
   const Application &app;
public:
   ActorState(EntitySystem *system, Entity *e)
      :m_system(system), app(*IOC.resolve<Application>()), e(e){}
   virtual void onEnter(){}
   virtual void onLeave(){}

   virtual void update(){}

   virtual void moveLeft(){}
   virtual void moveRight(){}
   virtual void moveUp(){}
   virtual void moveDown(){}
   
   virtual void stopLeft(){}
   virtual void stopRight(){}
   virtual void stopUp(){}
   virtual void stopDown(){}

   virtual void stun(){}
   virtual void unstun(){}

   virtual void face(Direction dir){}

   virtual void executeAction(Entity *e, ActionType type, Float2 target){}
   virtual void endAction(Entity *e, ActionType type, Float2 target){}
};
typedef StateMachine<ActorState> CharSM;

struct TActorComponent : public Component
{
   mutable std::shared_ptr<CharSM> sm;

   enum class MovementType : unsigned int
   {
      Up = 0,
      Down,
      Left,
      Right
   };

   mutable unsigned char moveFlags[4];

   TActorComponent()
      :sm(std::make_shared<CharSM>())
   {
      memset(moveFlags, 0, sizeof(moveFlags));
   }
};

REGISTER_COMPONENT(TActorComponent);

const static float MoveSpeed = 1.25f;

#pragma region Actor functions

void characterMove(Entity *e, TActorComponent::MovementType type)
{
   if(auto cc = e->get<TActorComponent>())
      cc->moveFlags[(unsigned int)type] = 1;
}

void characterStop(Entity *e, TActorComponent::MovementType type)
{
   if(auto cc = e->get<TActorComponent>())
      cc->moveFlags[(unsigned int)type] = 0;
}

void characterSetState(Entity *e, ActorState *state)
{
   if(auto cc = e->get<TActorComponent>())
      cc->sm->set(state);
}

void characterPushState(Entity *e, ActorState *state)
{
   if(auto cc = e->get<TActorComponent>())
      cc->sm->push(state);
}

void characterPopState(Entity *e)
{
   if(auto cc = e->get<TActorComponent>())
      cc->sm->pop();
}

void characterUpdateGroundMovement(Entity *e)
{
   float dt = IOC.resolve<Application>()->dt();

   if(auto vc = e->lock<VelocityComponent>())
   if(auto cc = e->get<TActorComponent>())
   {
      typedef TActorComponent::MovementType mt;

      if(cc->moveFlags[(unsigned int)mt::Up])
         vc->velocity.y = -MoveSpeed;
      else if(cc->moveFlags[(unsigned int)mt::Down])
         vc->velocity.y = MoveSpeed;
      else
         vc->velocity.y = 0;

      if(cc->moveFlags[(unsigned int)mt::Left])
         vc->velocity.x = -MoveSpeed;
      else if(cc->moveFlags[(unsigned int)mt::Right])
         vc->velocity.x = MoveSpeed;
      else
         vc->velocity.x = 0;
   }
}

void characterUpdateGroundSprite(Entity *e)
{
   if(auto tcc = e->get<TActorComponent>())
   if(auto cc = e->get<ActorComponent>())
   if(auto sc = e->lock<SpriteComponent>())
   {
      typedef TActorComponent::MovementType mt;

      bool up = tcc->moveFlags[(unsigned int)mt::Up];
      bool down = tcc->moveFlags[(unsigned int)mt::Down];
      bool left = tcc->moveFlags[(unsigned int)mt::Left];
      bool right = tcc->moveFlags[(unsigned int)mt::Right];

      if((up && sc->sprite == cc->upRunSprite) || 
         (down && sc->sprite == cc->downRunSprite) || 
         (left && sc->sprite == cc->leftRunSprite) || 
         (right && sc->sprite == cc->rightRunSprite))
         return;

      if(up) sc->sprite = cc->upRunSprite;
      else if(down) sc->sprite = cc->downRunSprite;
      else if(left) sc->sprite = cc->leftRunSprite;
      else if(right) sc->sprite = cc->rightRunSprite;

   }
}

void characterFace(Entity *e, Direction dir)
{
   if(auto cc = e->get<ActorComponent>())
   if(auto sc = e->lock<SpriteComponent>())
   {
      switch (dir)
      {
      case Direction::Up:
         if(sc->sprite != cc->upRunSprite) sc->sprite = cc->upRunSprite;
         break;
      case Direction::Down:
         if(sc->sprite != cc->downRunSprite) sc->sprite = cc->downRunSprite;
         break;
      case Direction::Left:
         if(sc->sprite != cc->leftRunSprite) sc->sprite = cc->leftRunSprite;
         break;
      case Direction::Right:
         if(sc->sprite != cc->rightRunSprite) sc->sprite = cc->rightRunSprite;
         break;
      default:
         break;
      }
   }
}

void characterAction(Entity *e, ActionType type, Float2 target, bool execute)
{
   if(auto ac = e->get<ActorComponent>())
   {
      auto &actList = IOC.resolve<GameData>()->actions;
      if(type == ActionType::MainHand && ac->mainHandAction)
      {
         auto iter = actList.find(ac->mainHandAction);
         if(iter != actList.end())
         {
            if(execute)
               iter->second->execute(e, target);
            else
               iter->second->end(e, target);
         }
      }
      else if(type == ActionType::OffHand && ac->offHandAction)
      {
         auto iter = actList.find(ac->offHandAction);
         if(iter != actList.end())
         {
            if(execute)
               iter->second->execute(e, target);
            else
               iter->second->end(e, target);
         }
      }
   }
}

#pragma endregion

class ActorManagerImpl : public Manager<ActorManagerImpl, ActorManager>
{
public:
   ActorManagerImpl()
   {
   }

   static void registerComponentCallbacks(Manager<ActorManagerImpl, ActorManager> &m)
   {
      
   }
   void onNew(Entity *e)
   {
      if(auto cc = e->get<ActorComponent>())
      {
         auto tact = TActorComponent();
         tact.sm->set(buildGroundState(cc->parent));

         cc->parent->add(tact);
      }
   }
   void onDelete(Entity *e)
   {
      if(auto cc = e->get<ActorComponent>())
         e->remove<TActorComponent>();            
   }

   ActorState *buildStunnedState(Entity *e)
   {
      class StunnedState : public ActorState
      {
         ActorManagerImpl *m_manager;
      public:
         StunnedState(ActorManagerImpl *manager, EntitySystem *system, Entity *e)
            :m_manager(manager), ActorState(system, e){}

         virtual void update()
         {  
            //characterUpdateGroundMovement(e);
            //characterUpdateGroundSprite(e);
         }

         virtual void moveLeft(){characterMove(e, TActorComponent::MovementType::Left);}
         virtual void moveRight(){characterMove(e, TActorComponent::MovementType::Right);}
         virtual void moveUp(){characterMove(e, TActorComponent::MovementType::Up);}
         virtual void moveDown(){characterMove(e, TActorComponent::MovementType::Down);}

         virtual void stopLeft(){characterStop(e, TActorComponent::MovementType::Left);}
         virtual void stopRight(){characterStop(e, TActorComponent::MovementType::Right);}
         virtual void stopUp(){characterStop(e, TActorComponent::MovementType::Up);}
         virtual void stopDown(){characterStop(e, TActorComponent::MovementType::Down);}

         virtual void unstun(){characterPopState(e);}
      };

      return new StunnedState(this, m_system, e);
   }

   ActorState *buildGroundState(Entity *e)
   {
      class GroundState : public ActorState
      {
         ActorManagerImpl *m_manager;
      public:
         GroundState(ActorManagerImpl *manager, EntitySystem *system, Entity *e)
            :m_manager(manager), ActorState(system, e){}

         virtual void update()
         {  
            characterUpdateGroundMovement(e);
            characterUpdateGroundSprite(e);
         }

         virtual void moveLeft(){characterMove(e, TActorComponent::MovementType::Left);}
         virtual void moveRight(){characterMove(e, TActorComponent::MovementType::Right);}
         virtual void moveUp(){characterMove(e, TActorComponent::MovementType::Up);}
         virtual void moveDown(){characterMove(e, TActorComponent::MovementType::Down);}

         virtual void stopLeft(){characterStop(e, TActorComponent::MovementType::Left);}
         virtual void stopRight(){characterStop(e, TActorComponent::MovementType::Right);}
         virtual void stopUp(){characterStop(e, TActorComponent::MovementType::Up);}
         virtual void stopDown(){characterStop(e, TActorComponent::MovementType::Down);}

         virtual void stun(){characterPushState(e, m_manager->buildStunnedState(e));}

         virtual void executeAction(Entity *e, ActionType type, Float2 target)
         {
            characterAction(e, type, target, true);
         }
         virtual void endAction(Entity *e, ActionType type, Float2 target)
         {
            characterAction(e, type, target, false);
         }
      };

      return new GroundState(this, m_system, e);
   }

   void update()   
   {
      for (auto comp : m_system->getComponentVector<TActorComponent>())
      {
         if(auto cc = comp.parent->get<TActorComponent>())
         if(auto &sm = *cc->sm)
         {
            sm->update();
         }
      }
   }

   template<typename Func>
   void characterStateFunction(Entity *e, Func && func)
   {
      if(auto cc = e->get<TActorComponent>())
         if(auto &sm = *cc->sm)
            func(sm);
   }

   void moveLeft(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->moveLeft();});}
   void moveRight(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->moveRight();});}
   void moveUp(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->moveUp();});}
   void moveDown(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->moveDown();});}

   void stopLeft(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->stopLeft();});}
   void stopRight(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->stopRight();});}
   void stopUp(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->stopUp();});}
   void stopDown(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->stopDown();});}

   void stun(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->stun();});}
   void unstun(Entity *e){characterStateFunction(e, [=](CharSM& sm){sm->unstun();});}

   void executeAction(Entity *e, ActionType type, Float2 target){characterStateFunction(e, [=](CharSM& sm){sm->executeAction(e, type, target);});}
   void endAction(Entity *e, ActionType type, Float2 target){characterStateFunction(e, [=](CharSM& sm){sm->endAction(e, type, target);});}

   void face(Entity *e, Direction dir){characterFace(e, dir);}
};

std::unique_ptr<ActorManager> buildActorManager()
{
   return std::unique_ptr<ActorManager>(new ActorManagerImpl());
}