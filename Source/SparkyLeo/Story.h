#pragma once

#include "CoreMinimal.h"
#include "GameTypes.h"

/**
 * The words, adapted from "The Adventures of Sparky and Leo" (Sparky_and_Leo/The_Adventures_of_
 * Sparky_and_Leo.pdf). Each chapter opens with a few story pages, plays, and closes with more.
 */
namespace Story
{
	struct FChapter
	{
		const TCHAR* Title;
		const TCHAR* Place;
		TArray<FLine> Intro;
		TArray<FLine> Outro;
	};

	constexpr int32 NumChapters = 9;

	inline const FChapter& Get(int32 Chapter)
	{
		static const TArray<FChapter> Chapters = {
			{
				TEXT("SPACE CATS UNITE"), TEXT("THE CRANBERRY PLASMA SYSTEM"),
				{
					{EWho::None, TEXT("Sparky, the genius cat with an IQ that could rival the top scientists on Earth, tapped his paw against the control panel of his meticulously crafted spaceship.")},
					{EWho::None, TEXT("Beside him, Leopold - or Leo, as he preferred - slouched in his seat. Leo was the epitome of average, unremarkable in every way, except for his friendship with a cat rocket scientist.")},
					{EWho::Sparky, TEXT("Leo, I hate to say it, but the atom crunch drive 6000 is on the fritz. And I'm almost out of cat food.")},
					{EWho::Leo, TEXT("Do we have to stop somewhere?")},
					{EWho::Sparky, TEXT("Indeed. We'll make our way to the Cranberry Plasma system. I've heard it's a bit sketchy, but they have the parts we need.")},
				},
				{
					{EWho::None, TEXT("Before they left, they were invited to a grand feast, where Sparky was asked to give a speech. He spoke of unity, creativity, and the importance of embracing differences, leaving the crowd in a contemplative silence.")},
					{EWho::Leo, TEXT("You know, Sparky, I may be average, but with you, life is anything but.")},
					{EWho::None, TEXT("Sparky purred. With the ship repaired and stocked, they set course for their next adventure, forever changed by the people of Cranberry Plasma and their odd yet endearing customs.")},
				},
			},
			{
				TEXT("REVELATIONS AND REFLECTIONS"), TEXT("CHINATOWN, SAN FRANCISCO - YEARS AGO"),
				{
					{EWho::Leo, TEXT("You know, Sparky, today got me thinking about the meaning of life and all. Why do we do what we do?")},
					{EWho::Sparky, TEXT("Ah, the meaning of life. For me, it's about discovery, innovation, and the pursuit of knowledge.")},
					{EWho::Leo, TEXT("You remember how we met, don't you, Sparky? You were a cage fighter, the toughest cat in all the land.")},
					{EWho::Sparky, TEXT("Ah, yes. Those were dark days, but they were necessary for my survival. I was hiding, you see.")},
				},
				{
					{EWho::Leo, TEXT("Hiding? From what?")},
					{EWho::Sparky, TEXT("I suppose it's time I told you the truth, Leo. I'm not from Earth. I'm from the Catnip Sector, about 500 light-years from here.")},
					{EWho::Leo, TEXT("You're... you're an alien?")},
					{EWho::Sparky, TEXT("In a manner of speaking, yes. I came to study Earth's inhabitants, but I was discovered. The cage fights gave me a way to blend in.")},
					{EWho::Sparky, TEXT("You, Leo, are the most extraordinary average person I've ever met.")},
					{EWho::Leo, TEXT("And you, Sparky, are the most extraordinary cat, or alien, or whatever you are. I wouldn't trade our adventures for anything.")},
				},
			},
			{
				TEXT("PIRATES!"), TEXT("DEEP SPACE"),
				{
					{EWho::None, TEXT("The ship's alarms blared, jolting both Sparky and Leo from their contemplative moment.")},
					{EWho::Sparky, TEXT("We've got company.")},
					{EWho::Leo, TEXT("Oh good, I could go for a cup of tea with a stranger.")},
					{EWho::Sparky, TEXT("Not the good kind of company. Pirates. The most feared and despised creature in the galaxy commands them: Captain Crotch Sniffer, from the Butt Licking Sector.")},
					{EWho::Sparky, TEXT("Leo, you must man the guns. I couldn't find a gun system that didn't need opposable thumbs. It's up to you. I'll deal with the boarders!")},
				},
				{
					{EWho::None, TEXT("The pirate ship fired one last shot, narrowly missing them as they disappeared into hyperspace.")},
					{EWho::Leo, TEXT("We made it.")},
					{EWho::Sparky, TEXT("Yes... but where are we?")},
					{EWho::None, TEXT("They were lost, but they were alive. Together, they would face whatever awaited them in this unknown region of space.")},
				},
			},
			{
				TEXT("CRISIS AND REPAIR"), TEXT("ABOARD SPARKY'S SHIP"),
				{
					{EWho::None, TEXT("After the thrilling escape from the pirates, Sparky felt the need for a quick cat bath and some soothing catnip.")},
					{EWho::Sparky, TEXT("Leo, I need you to man the controls. Keep an eye on things, would you?")},
					{EWho::Leo, TEXT("Sure thing, Sparky! I'll keep everything ship-shape!")},
					{EWho::None, TEXT("Sparky drifted into a light sleep... until the harsh sound of the ship's alarms shattered his peaceful slumber.")},
				},
				{
					{EWho::None, TEXT("With a triumphant purr, he reconnected the last wire, and the module hummed back to life. The alarms ceased their wailing.")},
					{EWho::Sparky, TEXT("We did it, Leo. We're safe.")},
					{EWho::Leo, TEXT("Thanks to you, Sparky. I don't know what I would have done without you.")},
					{EWho::None, TEXT("They had survived another crisis, and their bond had grown even stronger. Their adventure was far from over...")},
				},
			},
			{
				TEXT("FLUFFER-NUTTER"), TEXT("A LUSH GREEN PLANET IN UNCHARTED SPACE"),
				{
					{EWho::None, TEXT("With the ship repaired and once again spaceworthy, Sparky and Leo set their sights on a new destination. The unknown regions of space were full of uncharted planets.")},
					{EWho::None, TEXT("They soon found themselves orbiting a lush, green planet teeming with life. Its primitive society intrigued Sparky, while Leo was drawn to the natural beauty of the landscape.")},
					{EWho::Leo, TEXT("Sparky, look at it! It's so green! Can we land? Please?")},
					{EWho::Sparky, TEXT("For scientific purposes, Leo. Strictly scientific.")},
				},
				{
					{EWho::None, TEXT("The days that followed were tense. Sparky's instincts were constantly at odds with his friendship with Leo.")},
					{EWho::None, TEXT("But in the end they found a compromise, and with Fluffernutter safely onboard, they bid farewell to the green planet and its people.")},
					{EWho::None, TEXT("They were a team, bound by friendship and shared experiences. They had even navigated the complexities of interspecies relationships.")},
				},
			},
			{
				TEXT("FLUFFERNUTTER'S REVENGE"), TEXT("ABOARD SPARKY'S SHIP"),
				{
					{EWho::None, TEXT("As Sparky and Leo continued their journey through the unknown regions of space, they revelled in the camaraderie, excitement, and endless possibilities before them.")},
					{EWho::Sparky, TEXT("Something's not right, Leo.")},
					{EWho::Leo, TEXT("What is it, Sparky?")},
					{EWho::Sparky, TEXT("It's the pirates! They've followed us through hyperspace!")},
					{EWho::None, TEXT("Outnumbered and outgunned, Sparky and Leo's ship was soon overwhelmed. They were boarded, their defences breached...")},
				},
				{
					{EWho::None, TEXT("They continued on their journey, knowing that they had faced one of the greatest threats in the galaxy and emerged victorious.")},
					{EWho::None, TEXT("Thanks to Fluffernutter, their team was stronger than ever. One thing was certain: even the smallest among them could make a big difference.")},
				},
			},
			{
				TEXT("THE LABYRINTH OF TIME"), TEXT("THE FORBIDDEN REGION"),
				{
					{EWho::None, TEXT("Their adventures led them to a part of the galaxy marked as uncharted and forbidden by many spacefaring civilizations: the Labyrinth of Time.")},
					{EWho::None, TEXT("A cosmic anomaly, a maze-like region of space filled with gravitational distortions, wormholes, and temporal rifts, where time itself twists and folds.")},
					{EWho::Leo, TEXT("Being the most average person ever, I'd like to say I'm a little apprehensive.")},
					{EWho::Sparky, TEXT("Noted. We're going in.")},
				},
				{
					{EWho::None, TEXT("Inside the Labyrinth they encountered visions of their pasts and possible futures, and echoes of themselves from parallel timelines.")},
					{EWho::None, TEXT("The Labyrinth of Time awaited, and Sparky, Leo, and Fluffernutter were ready to answer its call.")},
				},
			},
			{
				TEXT("RIDDLES OF THE ANCIENTS"), TEXT("THE HEART OF THE LABYRINTH"),
				{
					{EWho::None, TEXT("As they ventured deeper into the Labyrinth, they stumbled upon a chamber, suspended in a timeless void.")},
					{EWho::None, TEXT("At its centre, a holographic interface projected ancient symbols and cryptic riddles, left by a long-lost civilization to test those who seek the Labyrinth's secrets.")},
				},
				{
					{EWho::None, TEXT("They had faced the trials of an ancient and mysterious place, and emerged stronger, wiser, and more connected.")},
					{EWho::None, TEXT("They were explorers, adventurers, and seekers of truth, and they knew that their journey had only just begun.")},
				},
			},
			{
				TEXT("ANOTHER DIMENSION"), TEXT("BEYOND THE WORMHOLE"),
				{
					{EWho::Leo, TEXT("Are we ready for this, Sparky? This is unknown territory.")},
					{EWho::Sparky, TEXT("That's what makes it exciting, Leo. We're explorers. The unknown is our calling.")},
					{EWho::None, TEXT("They entered the wormhole, and reality twisted around them: a mixture of falling and flying, of being pulled apart and put back together.")},
				},
				{
					{EWho::None, TEXT("They learned to navigate the shifting gravity, to communicate with the luminous beings, and to understand the living energy that permeated everything.")},
					{EWho::None, TEXT("As they prepared to leave, they looked back at the shimmering wormhole: a gateway to endless possibilities.")},
					{EWho::None, TEXT("The adventure continued. The unknown awaited, and Sparky, Leo, and Fluffernutter were ready for whatever came next...")},
				},
			},
		};
		return Chapters[FMath::Clamp(Chapter, 1, NumChapters) - 1];
	}
}
