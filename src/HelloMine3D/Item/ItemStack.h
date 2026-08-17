#ifndef ITEMSTACK_H_INCLUDED
#define ITEMSTACK_H_INCLUDED

#include "Material.h"

/// @brief Determines if a player character is holding blocks or items, also determines placement behavior.
class ItemStack {
  public:
    ItemStack(const Material &material, int amount, int durability = -1);

    int add(int amount);
    void remove();
    void remove(int amount);
    bool damage(int amount = 1);

    int getNumInStack() const;
    int getDurability() const;
    int getMaxDurability() const;
    bool isDamageable() const;

    const Material &getMaterial() const;
    bool isEmpty() const;

  private:
    const Material *m_pMaterial = &Material::NOTHING;
    int m_numInStack = 0;
    int m_durability = 0;
};

#endif // ITEMSTACK_H_INCLUDED
