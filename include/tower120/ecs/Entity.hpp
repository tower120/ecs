#pragma once

#include <tuple>
#include <array>
#include <cstddef>

#include <range/v3/view/transform.hpp>

#include "util/heterogeneous_array.hpp"
#include "detail/common.hpp"

#include "IEntity.hpp"

namespace tower120::ecs{

    template <class ...Components>
    class Entity final : public IEntity {
        static_assert(sizeof...(Components) > 0, "Must be at least one Component.");
        static_assert(util::all_unique<Components...>, "Components must be unique.");
        static_assert((is_component<Components> && ...), "Components only!");

        template<class T, std::enable_if_t<is_entity<T>, int>>
        friend EntityType type_id() noexcept;
        template<class T, std::enable_if_t<is_entity<T>, int>>
        friend EntityType type_id(const T&) noexcept;

        static EntityType type_id() noexcept {
            const static EntityType type_id = util::monotonic_counter<EntityType, IEntity>::get();
            return type_id;
        }

        template<int I>
        using ComponentN = util::TypeN<I, Components...>;

        template<class Closure, std::size_t ...I>
        static void foreach_component_(Closure&& closure, std::index_sequence<I...>){
            (closure(std::integral_constant<std::size_t, I>{}), ...);
        }
        template<class Closure>
        static void foreach_component(Closure &&closure){
            foreach_component_(std::forward<Closure>(closure), std::index_sequence_for<Components...>{});
        }

        util::heterogeneous_array<Components...> m_components;

        using ComponentsOffsetTable = std::array<ComponentTypeOffset, sizeof...(Components) >;
        template<std::size_t ...I>
        static ComponentsOffsetTable make_components_offset_table(std::index_sequence<I...>) noexcept {
            return
            { std::make_pair(
                ComponentN<I>::type_id(),    // non-constxepr!
                decltype(m_components)::offset_table[I]
            )... };
        }
        inline static const ComponentsOffsetTable m_components_offset_table_ = make_components_offset_table(std::index_sequence_for<Components...>{});

        template<int I>
        static void register_entity() noexcept {
            using Component = ComponentN<I>;
            constexpr const auto offset = decltype(m_components)::offset_table[I];
            static_assert(offset < std::numeric_limits<detail::entity_offset_t>::max(), "Entity size too big. Increase entity_offset_t.");

            Component::offset_for_entity().resize(type_id()+1, -1);
            Component::offset_for_entity()[type_id()] = offset;
        }

        static void register_entities() noexcept {
            foreach_component([&](auto I){ register_entity<I.value>(); });
        }

        static inline const bool registered = [](){
            register_entities();
            m_components_offset_table().resize(type_id()+1);
            m_components_offset_table()[type_id()] = m_components_offset_table_;
            return true;
        }();

    public:
        template<class Component>
        Component& get() noexcept {
            static_assert(util::has_type<Component, Components...>, "Component does not exists in Entity.");
            constexpr const int N = util::type_index<Component, Components...>;
            return m_components.template get<Component>(N);
        }

        Entity() noexcept
            : IEntity(type_id())
        {
            // register before use
            (void)registered;
            m_components_ptr = &m_components.data();

            // make sure that IComponent and Component have the same address
            // otherwise - make runtime offset table for IComponent (once - on construction)
            util::static_for<sizeof...(Components)>([&](auto n){
                using Component = ComponentN<n.value>;
                assert(&get<Component>() == static_cast<IComponent*>(&get<Component>()));
            });
        }

        // TODO: cast operator

        // TODO : update EntityBase::m_components on move / copy
        Entity(const Entity&) = delete;
        Entity(Entity&&) = delete;
    };

    template<class T, std::enable_if_t<is_entity<T>, int> = 0>
    EntityType type_id(const T&) noexcept {
        return T::type_id();
    }
    template<class T, std::enable_if_t<is_entity<T>, int> = 0>
    EntityType type_id() noexcept {
        return T::type_id();
    }
}