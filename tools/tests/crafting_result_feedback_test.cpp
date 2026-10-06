#include "../../src/HelloMine3D/Presentation/CraftingResultFeedback.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    using Tone = CraftingResultFeedback::Tone;

    void acknowledge(CraftingResultFeedback &feedback,
                     const CraftingCommitResult &result)
    {
        feedback.submit(result.message,
            result.succeeded() ? Tone::Success : Tone::Failure);
    }
}

int main(int argc, char **argv)
{
    unsigned checks = 0;
    const auto check = [&](bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
        ++checks;
    };
    try
    {
        check(argc == 2, "an explicit production recipe path is required");
        std::ifstream input(argv[1], std::ios::binary);
        check(static_cast<bool>(input), "production recipe source is unavailable");
        RecipeRegistry recipes;
        recipes.freeze({{argv[1], std::string(
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>())}});
        const RecipeDefinition *planks = recipes.find("hellomine:oak_planks");
        check(planks != nullptr, "production oak-planks recipe is unavailable");
        check(planks->outputMaterialId == Material::ID::OakPlank &&
              planks->outputCount == 4,
              "test no longer exercises the production four-plank exchange");

        // Exercise the same feedback against both actual UI crafting grids.
        for (const int gridSize : {CraftingSession::PlayerGridSize,
                                  CraftingSession::WorkbenchGridSize})
        {
            CraftingSession session(gridSize);
            Inventory inventory;
            CraftingResultFeedback feedback;
            check(session.loadRecipe(*planks), "production recipe did not fit grid");
            check(inventory.addItem(Material::OAK_BARK_BLOCK, 1) == 1,
                  "unable to provision last ingredient");
            const auto ready = session.preview(recipes, inventory);
            check(ready.ready() && ready.maxCrafts == 1,
                  "last-ingredient production preview is not ready for one craft");
            const auto committed = session.commit(recipes, inventory, ready, 1);
            check(committed.succeeded() && committed.outputAdded == 4,
                  "last-ingredient production exchange failed");
            acknowledge(feedback, committed);
            const auto after = session.preview(recipes, inventory);
            const auto shown = feedback.view(after);
            check(inventory.count(Material::ID::OakBark) == 0 &&
                  inventory.count(Material::ID::OakPlank) == 4,
                  "successful exchange did not conserve real input and output");
            check(shown.previewStatus == CraftingPreviewStatus::MissingIngredients &&
                  !shown.ready() && shown.maximumCrafts == 0,
                  "recent success hides the current missing-ingredients reason");
            check(shown.recentTone == Tone::Success &&
                  shown.recentMessage == committed.message,
                  "last-ingredient success acknowledgement was lost");
            feedback.advance(CraftingResultFeedback::LifetimeSeconds, true);
            check(feedback.view(after).recentMessage.empty() &&
                  feedback.view(after).previewStatus == after.status,
                  "success expiry discarded the current production reason");

            // A real exchange can fill output capacity while ingredients remain.
            Inventory packed(5);
            packed.applySaveState({{Material::ID::OakBark, 3},
                                   {Material::ID::OakPlank, 95},
                                   {Material::ID::Dirt, 99},
                                   {Material::ID::Stone, 99},
                                   {Material::ID::Sand, 99}}, 0);
            const auto capacity = session.preview(recipes, packed);
            check(capacity.ready() && capacity.maxCrafts == 1,
                  "production capacity preview is not ready for one craft");
            const auto filled = session.commit(recipes, packed, capacity, 1);
            check(filled.succeeded(), "capacity-filling production exchange failed");
            acknowledge(feedback, filled);
            const auto full = session.preview(recipes, packed);
            check(full.status == CraftingPreviewStatus::OutputFull &&
                  packed.count(Material::ID::OakBark) == 2 &&
                  packed.count(Material::ID::OakPlank) == 99,
                  "production exchange did not leave an output-full preview");
            check(feedback.view(full).previewStatus == CraftingPreviewStatus::OutputFull &&
                  !feedback.view(full).ready() &&
                  feedback.view(full).recentTone == Tone::Success,
                  "recent success hides the current output-capacity reason");
            const auto fullState = packed.getSaveState();
            const auto fullRevision = packed.revision();
            const auto fullSessionVersion = session.version();
            const auto rejectedFull = session.commit(recipes, packed, full, 1);
            check(rejectedFull.status == CraftingCommitStatus::OutputFull,
                  "full-output production failure was not retained");
            acknowledge(feedback, rejectedFull);
            check(packed.getSaveState() == fullState && packed.revision() == fullRevision &&
                  session.version() == fullSessionVersion,
                  "failed capacity exchange changed inventory or crafting state");
            check(feedback.view(full).recentMessage == rejectedFull.message &&
                  feedback.view(full).recentTone == Tone::Failure &&
                  feedback.view(full).previewStatus == CraftingPreviewStatus::OutputFull,
                  "failure feedback replaced the live capacity reason");

            // A stale real inventory fails atomically, then a fresh operation
            // replaces the failure and receives the complete visible lifetime.
            Inventory retry;
            check(retry.addItem(Material::OAK_BARK_BLOCK, 3) == 3,
                  "unable to provision retry ingredients");
            const auto stale = session.preview(recipes, retry);
            check(retry.addItem(Material::OAK_BARK_BLOCK, 1) == 1,
                  "unable to produce real stale inventory revision");
            const auto retryState = retry.getSaveState();
            const auto retryRevision = retry.revision();
            const auto rejected = session.commit(recipes, retry, stale, 1);
            check(rejected.status == CraftingCommitStatus::StaleInventory &&
                  retry.getSaveState() == retryState && retry.revision() == retryRevision,
                  "stale production exchange was not atomic");
            acknowledge(feedback, rejected);
            auto current = session.preview(recipes, retry);
            check(feedback.view(current).ready() &&
                  feedback.view(current).maximumCrafts == current.maxCrafts &&
                  feedback.view(current).recentTone == Tone::Failure,
                  "stale-operation feedback hides a currently valid recipe");
            feedback.advance(3.5f, true);
            const auto retried = session.commit(recipes, retry, current, 1);
            check(retried.succeeded(), "fresh production retry failed");
            acknowledge(feedback, retried);
            current = session.preview(recipes, retry);
            check(feedback.view(current).ready() &&
                  feedback.view(current).maximumCrafts == 3 &&
                  feedback.view(current).recentMessage == retried.message &&
                  feedback.view(current).recentTone == Tone::Success,
                  "successful retry did not refresh live quantity and acknowledgement");
            feedback.advance(.5f, true);
            check(!feedback.view(current).recentMessage.empty(),
                  "new acknowledgement inherited the old failure's remaining lifetime");
            feedback.advance(3.5f, true);
            check(feedback.view(current).recentMessage.empty(),
                  "refreshed acknowledgement did not expire after four visible seconds");

            // Identical consecutive successful operations also restart the timer.
            current = session.preview(recipes, retry);
            const auto again = session.commit(recipes, retry, current, 1);
            check(again.succeeded(), "consecutive production craft failed");
            acknowledge(feedback, again);
            feedback.advance(3.5f, true);
            current = session.preview(recipes, retry);
            const auto repeated = session.commit(recipes, retry, current, 1);
            check(repeated.succeeded() && repeated.message == again.message,
                  "production repeat did not exercise an identical success message");
            acknowledge(feedback, repeated);
            current = session.preview(recipes, retry);
            feedback.advance(.5f, true);
            check(!feedback.view(current).recentMessage.empty(),
                  "identical acknowledgement did not restart its lifetime");

            // Paused/hidden time and invalid deltas cannot consume visible time.
            acknowledge(feedback, repeated);
            feedback.advance(100.f, false);
            for (const float invalid : {0.f, -1.f,
                    std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity()})
            {
                feedback.advance(invalid, true);
                check(!feedback.view(current).recentMessage.empty(),
                      "pause or invalid delta consumed the acknowledgement");
            }
            feedback.advance(3.5f, true);
            check(!feedback.view(current).recentMessage.empty(),
                  "paused or invalid deltas reduced the four-second lifetime");
            feedback.advance(.5f, true);
            check(feedback.view(current).recentMessage.empty(),
                  "valid resumed presentation time did not expire the acknowledgement");
            acknowledge(feedback, repeated);
            feedback.advance(std::numeric_limits<float>::max(), true);
            check(feedback.view(current).recentMessage.empty(),
                  "large finite delta did not expire safely");

            // These are the reset operations used at close and world rebinding.
            // Reopening has a fresh real session and never sees another panel's
            // previous operation, while its own preview remains authoritative.
            acknowledge(feedback, repeated);
            feedback.clear();
            CraftingSession reopened(gridSize);
            const auto empty = reopened.preview(recipes, retry);
            check(feedback.view(empty).recentMessage.empty() &&
                  feedback.view(empty).recentTone == Tone::Information &&
                  feedback.view(empty).previewStatus == CraftingPreviewStatus::NoMatch,
                  "close/reopen reset retained a previous panel acknowledgement");
            feedback.submit("Recipe loaded", Tone::Information);
            feedback.clear();
            Inventory anotherWorld;
            check(anotherWorld.addItem(Material::OAK_BARK_BLOCK, 2) == 2 &&
                  reopened.loadRecipe(*planks),
                  "unable to provision the next world session");
            const auto nextWorld = reopened.preview(recipes, anotherWorld);
            check(feedback.view(nextWorld).recentMessage.empty() &&
                  feedback.view(nextWorld).maximumCrafts == 2,
                  "world reset retained previous acknowledgement or quantity");
            feedback.submit("", Tone::Success);
            check(feedback.view(nextWorld).recentMessage.empty() &&
                  feedback.view(nextWorld).ready(),
                  "empty acknowledgement changed the authoritative preview");
        }
        std::cout << "PASS crafting result feedback: " << checks
                  << " checks; real Base.recipe, Inventory, CraftingSession; no GPU\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
