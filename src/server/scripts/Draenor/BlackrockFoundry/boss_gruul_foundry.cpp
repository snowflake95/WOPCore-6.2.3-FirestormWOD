////////////////////////////////////////////////////////////////////////////////
///
///  MILLENIUM-STUDIO
///  Copyright 2015 Millenium-studio SARL
///  All Rights Reserved.
///
////////////////////////////////////////////////////////////////////////////////

# include "blackrock_foundry.hpp"

Position const g_CenterPos = { 373.618f, 3583.1f, 279.755f, 4.60341f };

/// Gruul - 76877
class boss_gruul_foundry : public CreatureScript
{
    public:
        boss_gruul_foundry() : CreatureScript("boss_gruul_foundry") { }

        enum eSpells
        {
            /// Misc
            GruulBonus              = 177529,
            RageRegenerationDisable = 173204,
            RageRegenerationAura    = 155534,
            ModScale95_120Pct       = 123978,
            PristineOre             = 165186,
            CaveInDoT               = 173192,
            /// Inferno Slice
            InfernoSlice            = 155080,
            InfernoStrike           = 162322,
            /// Overwhelming Blows
            OverwhelmingBlowsAura   = 155077,
            OverwhelmingBlowsProc   = 155078,
            /// Destructive Rampage
            SpellDestructiveRampage = 155539,
            /// Petrifying Slam
            PetrifyingSlam          = 155326,   ///< Triggers 155323: Knock Back + Periodic
            Shatter                 = 158867,
            PetrifiedStun           = 155506,
            PetrifyStacks           = 155330,
            /// Overhead Smash
            OverheadSmash           = 155301,
            OverheadSmashAoE        = 173190
        };

        enum eEvents
        {
            EventInfernoSlice = 1,
            EventOverwhelmingBlows,
            EventCaveIn,
            EventPetrifyingSlam,
            EventOverheadSmash,
            EventDestructiveRampage,
            EventBerserker
        };

        enum eCosmeticEvents
        {
            CosmeticEventOverheadSmash = 1,
            CosmeticEventEndOfDestructiveRampage
        };

        enum eActions
        {
            ActionInfernoSlice,
            ActionPetrification,
            ActionShatter
        };

        enum eCreatures
        {
            TriggerCaveIn       = 86596,
            PristineTrueIronOre = 82074
        };

        enum eTalks
        {
            Intro,
            Aggro,
            Petrify,
            DestructiveRampage,
            Berserk,
            Slay,
            Death,
            DestructiveRampageStart,
            DestructiveRampageEnd
        };

        struct boss_gruul_foundryAI : public BossAI
        {
            boss_gruul_foundryAI(Creature* p_Creature) : BossAI(p_Creature, eFoundryDatas::DataGruul)
            {
                m_Instance  = p_Creature->GetInstanceScript();
                m_IntroDone = false;
            }

            EventMap m_Events;
            EventMap m_CosmeticEvents;

            bool m_IntroDone;

            InstanceScript* m_Instance;

            std::list<uint64> m_PetrifiedTargets;

            void Reset() override
            {
                ClearDelayedOperations();

                m_Events.Reset();
                m_CosmeticEvents.Reset();

                summons.DespawnAll();

                me->SetReactState(ReactStates::REACT_AGGRESSIVE);

                me->RemoveAura(eFoundrySpells::Berserker);
                me->RemoveAura(eSpells::RageRegenerationAura);

                me->CastSpell(me, eSpells::OverwhelmingBlowsAura, true);
                me->CastSpell(me, eSpells::RageRegenerationDisable, true);

                me->setPowerType(Powers::POWER_MANA);
                me->SetPower(Powers::POWER_MANA, 0);
                me->SetMaxPower(Powers::POWER_MANA, 100);

                _Reset();

                m_PetrifiedTargets.clear();

                std::list<Creature*> l_TrueIronOres;
                me->GetCreatureListWithEntryInGrid(l_TrueIronOres, eCreatures::PristineTrueIronOre, 100.0f);

                for (Creature* l_IronOre : l_TrueIronOres)
                {
                    l_IronOre->CastSpell(me, eSpells::ModScale95_120Pct, true);
                    l_IronOre->CastSpell(me, eSpells::PristineOre, true);
                }
            }

            bool CanRespawn() override
            {
                return false;
            }

            void DoAction(int32 const p_Action) override
            {
                switch (p_Action)
                {
                    case eActions::ActionInfernoSlice:
                    {
                        if (m_Events.HasEvent(eEvents::EventInfernoSlice))
                            break;

                        m_Events.ScheduleEvent(eEvents::EventInfernoSlice, 50);
                        break;
                    }
                    case eActions::ActionPetrification:
                    {
                        for (uint64 l_Guid : m_PetrifiedTargets)
                        {
                            if (Unit* l_Target = Unit::GetUnit(*me, l_Guid))
                            {
                                l_Target->CastSpell(l_Target, eSpells::PetrifiedStun, true);
                                l_Target->RemoveAura(eSpells::PetrifyStacks);
                            }
                        }

                        break;
                    }
                    case eActions::ActionShatter:
                    {
                        for (uint64 l_Guid : m_PetrifiedTargets)
                        {
                            if (Unit* l_Target = Unit::GetUnit(*me, l_Guid))
                            {
                                l_Target->RemoveAura(eSpells::PetrifiedStun);
                                me->CastSpell(l_Target, eSpells::Shatter, true);
                            }
                        }

                        m_PetrifiedTargets.clear();
                        break;
                    }
                    default:
                        break;
                }
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                _EnterCombat();

                Talk(eTalks::Aggro);

                if (m_Instance != nullptr)
                    m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_ENGAGE, me, 1);

                me->CastSpell(me, eSpells::RageRegenerationAura, true);

                m_Events.ScheduleEvent(eEvents::EventCaveIn, 12 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventPetrifyingSlam, 22 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventOverheadSmash, 44 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventDestructiveRampage, 100 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventBerserker, (IsMythic() || IsHeroic()) ? 360 * TimeConstants::IN_MILLISECONDS : 480 * TimeConstants::IN_MILLISECONDS);
            }

            void KilledUnit(Unit* p_Killed) override
            {
                if (p_Killed->GetTypeId() == TypeID::TYPEID_PLAYER)
                    Talk(eTalks::Slay);
            }

            void JustDied(Unit* p_Killer) override
            {
                _JustDied();

                m_Events.Reset();
                m_CosmeticEvents.Reset();

                summons.DespawnAll();

                if (m_Instance != nullptr)
                {
                    m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_DISENGAGE, me);

                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::OverwhelmingBlowsProc);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::InfernoSlice);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::InfernoStrike);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::PetrifyStacks);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::PetrifiedStun);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::CaveInDoT);

                    CastSpellToPlayers(me->GetMap(), me, eSpells::GruulBonus, true);
                }
            }

            void EnterEvadeMode() override
            {
                me->RemoveAllAuras();

                me->InterruptNonMeleeSpells(true);

                /// Just in case, to prevent the fail Return to Home
                me->ClearUnitState(UnitState::UNIT_STATE_ROOT);
                me->ClearUnitState(UnitState::UNIT_STATE_DISTRACTED);
                me->ClearUnitState(UnitState::UNIT_STATE_STUNNED);

                CreatureAI::EnterEvadeMode();

                summons.DespawnAll();

                if (m_Instance != nullptr)
                {
                    m_Instance->SetBossState(eFoundryDatas::DataGruul, EncounterState::FAIL);

                    m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_DISENGAGE, me);

                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::OverwhelmingBlowsProc);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::InfernoSlice);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::InfernoStrike);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::PetrifyStacks);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::PetrifiedStun);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::CaveInDoT);
                }
            }

            void MoveInLineOfSight(Unit* p_Who) override
            {
                if (p_Who->GetTypeId() != TypeID::TYPEID_PLAYER)
                    return;

                if (m_IntroDone)
                    return;

                if (p_Who->GetDistance(me) > 60.0f)
                    return;

                Talk(eTalks::Intro);
                m_IntroDone = true;
            }

            void MovementInform(uint32 p_Type, uint32 p_ID) override
            {
                if (p_Type != MovementGeneratorType::POINT_MOTION_TYPE)
                    return;

                if (p_ID == eSpells::SpellDestructiveRampage)
                {
                    me->AttackStop();

                    m_CosmeticEvents.ScheduleEvent(eCosmeticEvents::CosmeticEventOverheadSmash, 2 * TimeConstants::IN_MILLISECONDS);
                }
            }

            void OnSpellCasted(SpellInfo const* p_SpellInfo) override
            {
                switch (p_SpellInfo->Id)
                {
                    case eSpells::InfernoSlice:
                    {
                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                            me->CastSpell(l_Target, eSpells::InfernoStrike, true);

                        break;
                    }
                    case eSpells::OverheadSmash:
                    {
                        me->CastSpell(me, eSpells::OverheadSmashAoE, true);
                        break;
                    }
                    default:
                        break;
                }
            }

            void SpellHitTarget(Unit* p_Target, SpellInfo const* p_SpellInfo) override
            {
                if (p_Target == nullptr)
                    return;

                switch (p_SpellInfo->Id)
                {
                    case eSpells::PetrifyingSlam:
                    {
                        m_PetrifiedTargets.push_back(p_Target->GetGUID());
                        break;
                    }
                    case eSpells::OverheadSmash:
                    {
                        AddTimedDelayedOperation(100, [this]() -> void
                        {
                            me->SetReactState(ReactStates::REACT_AGGRESSIVE);

                            if (!me->HasAura(eSpells::SpellDestructiveRampage))
                            {
                                if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                                {
                                    AttackStart(l_Target);

                                    me->GetMotionMaster()->Clear();
                                    me->GetMotionMaster()->MoveChase(l_Target);
                                }
                            }
                        });

                        break;
                    }
                    default:
                        break;
                }
            }

            void DamageDealt(Unit* p_Victim, uint32& p_Damage, DamageEffectType p_DamageType) override
            {
                if (p_DamageType != DamageEffectType::DIRECT_DAMAGE)
                    return;

                if (!m_Events.HasEvent(eEvents::EventOverwhelmingBlows))
                    m_Events.ScheduleEvent(eEvents::EventOverwhelmingBlows, 3 * TimeConstants::IN_MILLISECONDS);
            }

            void RegeneratePower(Powers p_Power, int32& p_Value) override
            {
                /// Gruul only regens by script
                p_Value = 0;
            }

            void UpdateAI(uint32 const p_Diff) override
            {
                UpdateOperations(p_Diff);

                m_CosmeticEvents.Update(p_Diff);

                switch (m_CosmeticEvents.ExecuteEvent())
                {
                    case eCosmeticEvents::CosmeticEventOverheadSmash:
                    {
                        float l_O = frand(0, 2 * M_PI);

                        me->SetFacingTo(l_O);
                        me->SetReactState(ReactStates::REACT_PASSIVE);

                        me->CastSpell(me, eSpells::OverheadSmash, false);

                        m_CosmeticEvents.ScheduleEvent(eCosmeticEvents::CosmeticEventOverheadSmash, 5 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eCosmeticEvents::CosmeticEventEndOfDestructiveRampage:
                    {
                        Talk(eTalks::DestructiveRampageEnd, me->GetGUID());

                        me->RemoveAura(eSpells::SpellDestructiveRampage);

                        if (Spell* l_CurrentSpell = me->GetCurrentSpell(CurrentSpellTypes::CURRENT_GENERIC_SPELL))
                        {
                            AddTimedDelayedOperation(l_CurrentSpell->GetCastTime() + 100, [this]() -> void
                            {
                                if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                                {
                                    AttackStart(l_Target);

                                    me->GetMotionMaster()->Clear();
                                    me->GetMotionMaster()->MoveChase(l_Target);
                                }
                            });
                        }
                        else
                        {
                            if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                            {
                                AttackStart(l_Target);

                                me->GetMotionMaster()->Clear();
                                me->GetMotionMaster()->MoveChase(l_Target);
                            }
                        }

                        m_CosmeticEvents.Reset();
                        break;
                    }
                    default:
                        break;
                }

                if (!UpdateVictim() || me->HasAura(eSpells::SpellDestructiveRampage))
                    return;

                m_Events.Update(p_Diff);

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                switch (m_Events.ExecuteEvent())
                {
                    case eEvents::EventInfernoSlice:
                    {
                        m_Events.CancelEvent(eEvents::EventOverwhelmingBlows);
                        me->CastSpell(me, eSpells::InfernoSlice, false);
                        break;
                    }
                    case eEvents::EventOverwhelmingBlows:
                    {
                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                            me->CastSpell(l_Target, eSpells::OverwhelmingBlowsProc, true);

                        break;
                    }
                    case eEvents::EventCaveIn:
                    {
                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM, 0, -10.0f))
                            me->SummonCreature(eCreatures::TriggerCaveIn, *l_Target);
                        else if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM))
                            me->SummonCreature(eCreatures::TriggerCaveIn, *l_Target);

                        m_Events.ScheduleEvent(eEvents::EventCaveIn, 30 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventPetrifyingSlam:
                    {
                        m_Events.CancelEvent(eEvents::EventOverwhelmingBlows);

                        me->CastSpell(me, eSpells::PetrifyingSlam, false);

                        AddTimedDelayedOperation(7800, [this]() -> void
                        {
                            DoAction(eActions::ActionPetrification);
                        });

                        AddTimedDelayedOperation(10800, [this]() -> void
                        {
                            DoAction(eActions::ActionShatter);
                        });

                        m_Events.ScheduleEvent(eEvents::EventPetrifyingSlam, 60 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventOverheadSmash:
                    {
                        m_Events.CancelEvent(eEvents::EventOverwhelmingBlows);

                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM))
                        {
                            float l_O = me->GetAngle(l_Target);

                            me->SetFacingTo(l_O);
                            me->SetReactState(ReactStates::REACT_PASSIVE);
                        }

                        me->CastSpell(me, eSpells::OverheadSmash, false);
                        m_Events.ScheduleEvent(eEvents::EventOverheadSmash, 34 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventDestructiveRampage:
                    {
                        m_Events.CancelEvent(eEvents::EventOverwhelmingBlows);

                        Talk(eTalks::DestructiveRampage);
                        Talk(eTalks::DestructiveRampageStart, me->GetGUID());

                        me->CastSpell(me, eSpells::SpellDestructiveRampage, true);

                        AddTimedDelayedOperation(100, [this]() -> void
                        {
                            me->SetReactState(ReactStates::REACT_PASSIVE);

                            me->GetMotionMaster()->Clear();
                            me->GetMotionMaster()->MovePoint(eSpells::SpellDestructiveRampage, g_CenterPos);
                        });

                        m_CosmeticEvents.ScheduleEvent(eCosmeticEvents::CosmeticEventEndOfDestructiveRampage, 30 * TimeConstants::IN_MILLISECONDS);
                        m_Events.ScheduleEvent(eEvents::EventDestructiveRampage, 110 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventBerserker:
                    {
                        me->CastSpell(me, eFoundrySpells::Berserker, true);
                        Talk(eTalks::Berserk);
                        break;
                    }
                    default:
                        break;
                }

                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new boss_gruul_foundryAI(p_Creature);
        }
};

/// Cave In - 86596
class npc_foundry_cave_in : public CreatureScript
{
    public:
        npc_foundry_cave_in() : CreatureScript("npc_foundry_cave_in") { }

        enum eSpell
        {
            CaveInAreaTrigger = 173191
        };

        struct npc_foundry_cave_inAI : public ScriptedAI
        {
            npc_foundry_cave_inAI(Creature* p_Creature) : ScriptedAI(p_Creature) { }

            void Reset() override
            {
                me->SetFlag(EUnitFields::UNIT_FIELD_FLAGS, eUnitFlags::UNIT_FLAG_IMMUNE_TO_PC | eUnitFlags::UNIT_FLAG_NON_ATTACKABLE | eUnitFlags::UNIT_FLAG_NOT_SELECTABLE | eUnitFlags::UNIT_FLAG_DISABLE_MOVE);
                me->SetFlag(EUnitFields::UNIT_FIELD_FLAGS2, eUnitFlags2::UNIT_FLAG2_DISABLE_TURN);

                me->SetReactState(ReactStates::REACT_PASSIVE);

                me->CastSpell(me, eSpell::CaveInAreaTrigger, true);

                me->DespawnOrUnsummon(90 * TimeConstants::IN_MILLISECONDS);
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_foundry_cave_inAI(p_Creature);
        }
};

/// Pristine True Iron Ore - 82074
class npc_foundry_pristine_true_iron_ore : public CreatureScript
{
    public:
        npc_foundry_pristine_true_iron_ore() : CreatureScript("npc_foundry_pristine_true_iron_ore") { }

        enum eSpells
        {
            ModScale95_120Pct   = 123978,
            PristineOre         = 165186
        };

        struct npc_foundry_pristine_true_iron_oreAI : public MS::AI::CosmeticAI
        {
            npc_foundry_pristine_true_iron_oreAI(Creature* p_Creature) : MS::AI::CosmeticAI(p_Creature)
            {
                m_Instance = p_Creature->GetInstanceScript();
                m_Clicked  = false;
            }

            InstanceScript* m_Instance;

            bool m_Clicked;

            void OnSpellCasted(SpellInfo const* p_SpellInfo) override
            {
                if (p_SpellInfo->Id == eSpells::PristineOre)
                {
                    m_Clicked = false;

                    me->SetFlag(EUnitFields::UNIT_FIELD_NPC_FLAGS, NPCFlags::UNIT_NPC_FLAG_SPELLCLICK);
                }
            }

            void OnSpellClick(Unit* p_Clicker) override
            {
                if (m_Instance != nullptr && !m_Clicked)
                {
                    m_Clicked = true;

                    m_Instance->SetData(eFoundryDatas::PristineTrueIronOres, 1);

                    me->RemoveAura(eSpells::ModScale95_120Pct);
                    me->RemoveAura(eSpells::PristineOre);
                }
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_foundry_pristine_true_iron_oreAI(p_Creature);
        }
};

/// Rage Regeneration - 155534
class spell_foundry_rage_regeneration : public SpellScriptLoader
{
    public:
        spell_foundry_rage_regeneration() : SpellScriptLoader("spell_foundry_rage_regeneration") { }

        class spell_foundry_rage_regeneration_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_foundry_rage_regeneration_AuraScript);

            enum eAction
            {
                ActionInfernoSlice
            };

            void OnTick(constAuraEffectPtr p_AurEff)
            {
                if (GetTarget() == nullptr)
                    return;

                if (Creature* l_Gruul = GetTarget()->ToCreature())
                {
                    if (!l_Gruul->IsAIEnabled)
                        return;

                    l_Gruul->EnergizeBySpell(l_Gruul, GetSpellInfo()->Id, (p_AurEff->GetTickNumber() % 3) ? 3 : 4, Powers::POWER_MANA);

                    if (l_Gruul->GetPower(Powers::POWER_MANA) >= 100)
                        l_Gruul->AI()->DoAction(eAction::ActionInfernoSlice);
                }
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_foundry_rage_regeneration_AuraScript::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_foundry_rage_regeneration_AuraScript();
        }
};

/// Inferno Slice - 155080
class spell_foundry_inferno_slice : public SpellScriptLoader
{
    public:
        spell_foundry_inferno_slice() : SpellScriptLoader("spell_foundry_inferno_slice") { }

        class spell_foundry_inferno_slice_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_foundry_inferno_slice_SpellScript);

            enum eSpell
            {
                TargetRestrict = 19114
            };

            uint8 m_TargetCount;

            bool Load() override
            {
                m_TargetCount = 0;
                return true;
            }

            void CorrectTargets(std::list<WorldObject*>& p_Targets)
            {
                if (p_Targets.empty())
                    return;

                SpellTargetRestrictionsEntry const* l_Restriction = sSpellTargetRestrictionsStore.LookupEntry(eSpell::TargetRestrict);
                if (l_Restriction == nullptr)
                    return;

                Unit* l_Caster = GetCaster();
                if (l_Caster == nullptr)
                    return;

                float l_Angle = 2 * M_PI / 360 * l_Restriction->ConeAngle;
                p_Targets.remove_if([l_Caster, l_Angle](WorldObject* p_Object) -> bool
                {
                    if (p_Object == nullptr)
                        return true;

                    if (!p_Object->isInFront(l_Caster, l_Angle))
                        return true;

                    return false;
                });

                m_TargetCount = (uint8)p_Targets.size();
            }

            void HandleDamage(SpellEffIndex p_EffIndex)
            {
                if (m_TargetCount)
                    SetHitDamage(GetHitDamage() / m_TargetCount);

                if (m_TargetCount < 4)
                {
                    if (Creature* l_Gruul = GetCaster()->ToCreature())
                    {
                        if (!l_Gruul->IsAIEnabled)
                            return;

                        /// In Mythic difficulty, if Inferno Slice fails to hit at least 4 targets, Gruul will instantly gain 50 Rage.
                        if (l_Gruul->GetMap()->IsMythic())
                            l_Gruul->EnergizeBySpell(l_Gruul, GetSpellInfo()->Id, 50, Powers::POWER_MANA);
                    }
                }
            }

            void Register() override
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_foundry_inferno_slice_SpellScript::CorrectTargets, EFFECT_0, TARGET_UNIT_CONE_ENEMY_104);
                OnEffectHitTarget += SpellEffectFn(spell_foundry_inferno_slice_SpellScript::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_foundry_inferno_slice_SpellScript();
        }
};

/// Cave In - 173191
class spell_foundry_cave_in : public SpellScriptLoader
{
    public:
        spell_foundry_cave_in() : SpellScriptLoader("spell_foundry_cave_in") { }

        class spell_foundry_cave_in_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_foundry_cave_in_AuraScript);

            enum eSpell
            {
                CaveInDoT = 173192
            };

            void OnUpdate(uint32 p_Diff)
            {
                if (Unit* l_Caster = GetCaster())
                {
                    std::list<Unit*> l_TargetList;
                    float l_Radius = 15.0f;

                    JadeCore::AnyUnfriendlyUnitInObjectRangeCheck l_Check(l_Caster, l_Caster, l_Radius);
                    JadeCore::UnitListSearcher<JadeCore::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(l_Caster, l_TargetList, l_Check);
                    l_Caster->VisitNearbyObject(l_Radius, l_Searcher);

                    l_TargetList.remove(l_Caster);

                    for (Unit* l_Iter : l_TargetList)
                    {
                        if (l_Iter->GetDistance(l_Caster) <= 4.5f)
                        {
                            if (!l_Iter->HasAura(eSpell::CaveInDoT, l_Caster->GetGUID()))
                                l_Caster->CastSpell(l_Iter, eSpell::CaveInDoT, true);
                        }
                        else
                        {
                            if (l_Iter->HasAura(eSpell::CaveInDoT, l_Caster->GetGUID()))
                                l_Iter->RemoveAura(eSpell::CaveInDoT, l_Caster->GetGUID());
                        }
                    }
                }
            }

            void OnRemove(constAuraEffectPtr p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Unit* l_Caster = GetCaster())
                {
                    std::list<Unit*> l_TargetList;
                    float l_Radius = 15.0f;

                    JadeCore::AnyUnfriendlyUnitInObjectRangeCheck l_Check(l_Caster, l_Caster, l_Radius);
                    JadeCore::UnitListSearcher<JadeCore::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(l_Caster, l_TargetList, l_Check);
                    l_Caster->VisitNearbyObject(l_Radius, l_Searcher);

                    l_TargetList.remove(l_Caster);

                    for (Unit* l_Iter : l_TargetList)
                    {
                        if (l_Iter->HasAura(eSpell::CaveInDoT, l_Caster->GetGUID()))
                            l_Iter->RemoveAura(eSpell::CaveInDoT, l_Caster->GetGUID());
                    }
                }
            }

            void Register() override
            {
                OnAuraUpdate += AuraUpdateFn(spell_foundry_cave_in_AuraScript::OnUpdate);
                OnEffectRemove += AuraEffectRemoveFn(spell_foundry_cave_in_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_AREATRIGGER, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_foundry_cave_in_AuraScript();
        }
};

/// Petrifying Slam - 155326
class spell_foundry_petrifying_slam_aoe : public SpellScriptLoader
{
    public:
        spell_foundry_petrifying_slam_aoe() : SpellScriptLoader("spell_foundry_petrifying_slam_aoe") { }

        class spell_foundry_petrifying_slam_aoe_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_foundry_petrifying_slam_aoe_SpellScript);

            bool Load() override
            {
                if (Unit* l_Caster = GetCaster())
                {
                    std::list<HostileReference*> l_ThreatList = l_Caster->getThreatManager().getThreatList();
                    uint32 l_Count = std::count_if(l_ThreatList.begin(), l_ThreatList.end(), [this, l_Caster](HostileReference* p_HostileRef) -> bool
                    {
                        Unit* l_Unit = Unit::GetUnit(*l_Caster, p_HostileRef->getUnitGuid());
                        if (l_Unit == nullptr)
                            return false;

                        if (l_Unit->GetTypeId() != TypeID::TYPEID_PLAYER)
                            return false;

                        if (!l_Unit->isAlive())
                            return false;

                        return true;
                    });

                    GetSpell()->SetSpellValue(SpellValueMod::SPELLVALUE_MAX_TARGETS, int32(l_Count / 3));
                    return true;
                }
                else
                    return false;
            }

            void Register() override { }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_foundry_petrifying_slam_aoe_SpellScript();
        }
};

/// Petrifying Slam - 155323
class spell_foundry_petrifying_slam : public SpellScriptLoader
{
    public:
        spell_foundry_petrifying_slam() : SpellScriptLoader("spell_foundry_petrifying_slam") { }

        class spell_foundry_petrifying_slam_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_foundry_petrifying_slam_AuraScript);

            enum eSpell
            {
                PetrifyStacks = 155330
            };

            void OnTick(constAuraEffectPtr p_AurEff)
            {
                if (Unit* l_Target = GetTarget())
                {
                    if (p_AurEff->GetTickNumber() < 3)
                        return;

                    l_Target->CastSpell(l_Target, eSpell::PetrifyStacks, true);
                }
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_foundry_petrifying_slam_AuraScript::OnTick, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_foundry_petrifying_slam_AuraScript();
        }
};

/// Overhead Smash - 155301
class spell_foundry_overhead_smash : public SpellScriptLoader
{
    public:
        spell_foundry_overhead_smash() : SpellScriptLoader("spell_foundry_overhead_smash") { }

        class spell_foundry_overhead_smash_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_foundry_overhead_smash_SpellScript);

            enum eSpell
            {
                TargetRestrict = 19159
            };

            void CorrectTargets(std::list<WorldObject*>& p_Targets)
            {
                if (p_Targets.empty())
                    return;

                SpellTargetRestrictionsEntry const* l_Restriction = sSpellTargetRestrictionsStore.LookupEntry(eSpell::TargetRestrict);
                if (l_Restriction == nullptr)
                    return;

                Unit* l_Caster = GetCaster();
                if (l_Caster == nullptr)
                    return;

                float l_Radius = GetSpellInfo()->Effects[EFFECT_0].CalcRadius(l_Caster);
                p_Targets.remove_if([l_Radius, l_Caster, l_Restriction](WorldObject* p_Object) -> bool
                {
                    if (p_Object == nullptr)
                        return true;

                    if (!p_Object->IsInAxe(l_Caster, l_Restriction->Width, l_Radius))
                        return true;

                    if (!p_Object->isInFront(l_Caster))
                        return true;

                    return false;
                });
            }

            void HandleKnockBack(SpellEffIndex p_EffIndex)
            {
                if (Unit* l_Boss = GetCaster())
                {
                    if (Unit* l_Target = GetHitUnit())
                    {
                        float l_Distance = l_Target->GetDistance(l_Boss);
                        int32 l_Damage = GetSpell()->GetDamage() * int32(l_Distance / 5.0f);

                        GetSpell()->SetDamage(l_Damage);
                    }
                }
            }

            void Register() override
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_foundry_overhead_smash_SpellScript::CorrectTargets, EFFECT_0, TARGET_UNIT_CONE_ENEMY_129);
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_foundry_overhead_smash_SpellScript::CorrectTargets, EFFECT_1, TARGET_UNIT_CONE_ENEMY_129);
                OnEffectHitTarget += SpellEffectFn(spell_foundry_overhead_smash_SpellScript::HandleKnockBack, EFFECT_1, SPELL_EFFECT_KNOCK_BACK);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_foundry_overhead_smash_SpellScript();
        }
};

void AddSC_boss_gruul_foundry()
{
    /// Boss
    new boss_gruul_foundry();

    /// NPCs
    new npc_foundry_cave_in();
    new npc_foundry_pristine_true_iron_ore();

    /// Spells
    new spell_foundry_rage_regeneration();
    new spell_foundry_inferno_slice();
    new spell_foundry_cave_in();
    new spell_foundry_petrifying_slam_aoe();
    new spell_foundry_petrifying_slam();
    new spell_foundry_overhead_smash();
}