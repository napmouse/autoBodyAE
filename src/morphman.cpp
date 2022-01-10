#include <morphman.h>

namespace Bodygen
{
	Morphman* Morphman::GetInstance()
	{
		static Morphman instance;
		return std::addressof(instance);
	};

	void Morphman::initClothingSliders()
	{
		auto container = Presets::PresetContainer::GetInstance();
		std::vector<Presets::slider>* clothingsliders;
		clothingsliders = &container->clothingsliders;
		// booba
		clothingsliders->push_back({ 0.0f, 0.0f, "BreastSideShape" });
		clothingsliders->push_back({ 0.0f, 0.0f, "BreastUnderDepth" });
		clothingsliders->push_back({ 1.0f, 1.0f, "BreastCleavage" });
		clothingsliders->push_back({ -0.1f, -0.05f, "BreastGravity2" });
		clothingsliders->push_back({ -0.2f, -0.35f, "BreastTopSlope" });
		clothingsliders->push_back({ 0.3f, 0.35f, "BreastsTogether" });
		clothingsliders->push_back({ -0.05f, -0.05f, "Breasts" });
		clothingsliders->push_back({ 0.15f, 0.15f, "BreastHeight" });

		// butt
		clothingsliders->push_back({ 0.0f, 0.0f, "ButtDimples" });
		clothingsliders->push_back({ 0.0f, 0.0f, "ButtUnderFold" });
		clothingsliders->push_back({ -0.05f, -0.05f, "AppleCheeks" });
		clothingsliders->push_back({ -0.05f, -0.05f, "Butt" });

		// torso
		clothingsliders->push_back({ 0.0f, 0.0f, "Clavicle_v2" });
		clothingsliders->push_back({ 1.0f, 1.0f, "NavelEven" });
		clothingsliders->push_back({ 0.0f, 0.0f, "HipCarved" });

		// nipples
		clothingsliders->push_back({ 0.0f, 0.0f, "NippleDip" });
		clothingsliders->push_back({ 0.0f, 0.0f, "NippleTip" });
		clothingsliders->push_back({ 0.0f, 0.0f, "NipplePuffy_v2" });
		clothingsliders->push_back({ -0.3f, -0.3f, "AreolaSize" });
		clothingsliders->push_back({ 0.2f, 0.2f, "NipBGone" });
		//clothingsliders->push_back({ -0.75, -0.75, "NippleManga" });
		clothingsliders->push_back({ 0.05f, 0.08f, "NippleDistance" });
		clothingsliders->push_back({ 0.0f, -0.1f, "NippleDown" });
		//clothingsliders->push_back({ -0.25f, -0.25f, "NipplePerkManga" });
		clothingsliders->push_back({ 0.0f, 0.0f, "NipplePerkiness" });
		//logger::trace("clothingsliders is {} elements long right here", clothingsliders->size());
		container->clothingUnprocessed.sliderlist = *clothingsliders;
	}

	Presets::completedbody Morphman::FinishClothing(RE::Actor* a_actor)
	{
		auto weight = GetWeight(a_actor);
		auto container = Presets::PresetContainer::GetInstance();
		auto morphman = Bodygen::Morphman::GetInstance();
		//jank
		Presets::bodypreset* clothingUnprocessed{ new Presets::bodypreset };
		Presets::completedbody* clothingmods{ new Presets::completedbody };

		clothingUnprocessed = &container->clothingUnprocessed;

		//logger::trace("We've acquired an unprocessed list of {} elements", clothingUnprocessed->sliderlist.size());
		*clothingmods = InterpolateAllValues(*clothingUnprocessed, weight);
		//logger::trace("The clothing list is {} elements long", clothingmods->nodelist.size());
		for (int i = 0; i < clothingmods->nodelist.size(); ++i) {
			float precalibration = clothingmods->nodelist[i].value;
			float tuningvalue = morphman->morphInterface->GetMorph(a_actor, clothingmods->nodelist[i].name.c_str(), "autoBody");
			logger::trace("The precalibration value is {} and the value to tune down by is {}", precalibration, tuningvalue);
			clothingmods->nodelist[i].value = precalibration - tuningvalue;
		}

		return *clothingmods;
	}

	bool Morphman::GetMorphInterface(SKEE::IBodyMorphInterface* a_morphInterface) { return a_morphInterface->GetVersion() ? morphInterface = a_morphInterface : false; }

	// checks to see if an actor has been generated already.
	bool Morphman::IsGenned(RE::Actor* a_actor) { return morphInterface->GetMorph(a_actor, "autoBody_processed", "autoBody") == 1.0f; }

	// sticks a preset onto an NPC. This is the core function of the plugin, pretty
	// much.
	void Morphman::ApplyPreset(RE::Actor* a_actor, std::vector<Presets::bodypreset> list)
	{
		//ensure that the list has at least one item to avoid divide by zero errors.
		if (list.size() == 0) {
			logger::trace("There are no presets in this list! We can't apply one!");
			morphInterface->SetMorph(a_actor, "autoBody_processed", "autoBody", 1.0f);
			return;
		}
		// select a random preset from the stack
		auto preset = Presets::FindRandomPreset(list);

		// get the weight of the actor
		auto actorWeight = GetWeight(a_actor);
		// apply their weight to the preset + the offset defined in the INI
		Presets::completedbody readybody = InterpolateAllValues(preset, actorWeight);

		// finally, clear off any sliders we may have put on them.
		morphInterface->ClearBodyMorphKeys(a_actor, "autoBody");
		// prep complete

		// apply the sliders to the NPC
		ApplySliderSet(a_actor, readybody, "autoBody");

		// mark the actor as generated, so we don't fuck up and generate them again
		morphInterface->SetMorph(a_actor, "autoBody_processed", "autoBody", 1.0f);
		logger::info("Preset [{}] was applied to {}", readybody.presetname, a_actor->GetName());
		// store the actor's info just in case we need it later. Does not store it if
		// we already have done so. completedcharacter finished{readybody, a_actor,
		// a_actor->GetFormID(), a_actor->GetName(), actorWeight};
		// actorList.push_back(finished);

		return;
	}

	// apply a flattened slider (i.e. a single value and a name, i.e. what the morph
	// interface takes in), plus a morph key
	void Morphman::ApplySlider(RE::Actor* a_actor, Presets::flattenedslider slider, const char* key)
	{
		// logger::trace("Morph {} with value {} is being applied to actor {}",
		// slider.name, slider.value, a_actor->GetName());
		morphInterface->SetMorph(a_actor, slider.name.c_str(), key, slider.value);
	}

	// apply an entire sliderset to a person.
	void Morphman::ApplySliderSet(RE::Actor* a_actor, Presets::completedbody body, const char* key)
	{
		// logger::trace("Applying sliderset to actor... {}", body.nodelist.size());
		for (Presets::flattenedslider node : body.nodelist) {
			// logger::trace("Applying slider {} to the actor", node.name);
			ApplySlider(a_actor, node, key);
		}

		morphInterface->ApplyBodyMorphs(a_actor, true);
	}

	// apply or remove clothing sliders from an actor.
	void Morphman::ProcessClothing(RE::Actor* a_actor, bool unequip = false)
	{
		auto sexint = a_actor->GetActorBase()->GetSex();
		if (sexint == 1) {
			auto modifiers = FinishClothing(a_actor);
			logger::trace("Modifiers is {} big", modifiers.nodelist.size());
			if (unequip) {
				logger::trace("Removing clothing morphs from actor {}!", a_actor->GetName());
				morphInterface->ClearBodyMorphKeys(a_actor, "autoBodyClothes");
			} else {
				logger::trace("Applying clothing morphs to actor {}!", a_actor->GetName());
				ApplySliderSet(a_actor, modifiers, "autoBodyClothes");
			}
		}
	}

	// clears all changes we have made to an actor
	void Morphman::FlushActor(RE::Actor* a_actor)
	{
		// clear the morphs.
		morphInterface->ClearBodyMorphKeys(a_actor, "autoBody");

		morphInterface->ApplyBodyMorphs(a_actor, false);
		morphInterface->UpdateModelWeight(a_actor, true);
	}

	// how fat are they? lul
	float Morphman::GetWeight(RE::Actor* a_actor)
	{
		//logger::trace("getting actor weight.");
		return a_actor->GetActorBase()->GetWeight() / 100.0f;
	}
};  // namespace Bodygen
