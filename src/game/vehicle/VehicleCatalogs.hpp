#pragma once

#include "VehiclePaintTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Tutones::Game::VehicleCatalogs
{
    struct IndexedName final
    {
        int value{};
        const char* name{};
    };

    struct RgbName final
    {
        const char* name{};
        std::uint8_t red{};
        std::uint8_t green{};
        std::uint8_t blue{};
    };

    inline constexpr std::array<const char*, 23> VehicleClassNames{{
        "Compact", "Sedan", "SUV", "Coupe", "Muscle", "Sport Classic", "Sport", "Super",
        "Motorcycle", "Off-road", "Industrial", "Utility", "Van", "Cycle", "Boat", "Helicopter",
        "Plane", "Service", "Emergency", "Military", "Commercial", "Rail", "Open Wheel",
    }};

    inline constexpr std::array<IndexedName, 13> WheelTypes{{
        {0, "Sport"}, {1, "Muscle"}, {2, "Lowrider"}, {3, "SUV"}, {4, "Offroad"},
        {5, "Tuner"}, {6, "Bike Wheels"}, {7, "High End"}, {8, "Benny's Original"},
        {9, "Benny's Bespoke"}, {10, "Open Wheel"}, {11, "Street"}, {12, "Track"},
    }};

    inline constexpr auto ClassicColors = std::to_array<IndexedName>({
        {0,"Black"},{147,"Carbon Black"},{1,"Graphite"},{11,"Anthracite Black"},{2,"Black Steel"},
        {3,"Dark Steel"},{4,"Silver"},{5,"Bluish Silver"},{6,"Rolled Steel"},{7,"Shadow Silver"},
        {8,"Stone Silver"},{9,"Midnight Silver"},{10,"Cast Iron Silver"},{27,"Red"},{28,"Torino Red"},
        {29,"Formula Red"},{150,"Lava Red"},{30,"Blaze Red"},{31,"Grace Red"},{32,"Garnet Red"},
        {33,"Sunset Red"},{34,"Cabernet Red"},{143,"Wine Red"},{35,"Candy Red"},{135,"Hot Pink"},
        {137,"Pfister Pink"},{136,"Salmon Pink"},{36,"Sunrise Orange"},{38,"Orange"},{138,"Bright Orange"},
        {99,"Gold"},{90,"Bronze"},{88,"Yellow"},{89,"Race Yellow"},{91,"Dew Yellow"},
        {49,"Dark Green"},{50,"Racing Green"},{51,"Sea Green"},{52,"Olive Green"},{53,"Bright Green"},
        {54,"Gasoline Green"},{92,"Lime Green"},{141,"Midnight Blue"},{61,"Galaxy Blue"},{62,"Dark Blue"},
        {63,"Saxon Blue"},{64,"Blue"},{65,"Mariner Blue"},{66,"Harbor Blue"},{67,"Diamond Blue"},
        {68,"Surf Blue"},{69,"Nautical Blue"},{73,"Racing Blue"},{70,"Ultra Blue"},{74,"Light Blue"},
        {96,"Chocolate Brown"},{101,"Bison Brown"},{95,"Creek Brown"},{94,"Feltzer Brown"},{97,"Maple Brown"},
        {103,"Beechwood Brown"},{104,"Sienna Brown"},{98,"Saddle Brown"},{100,"Moss Brown"},{102,"Woodbeech Brown"},
        {105,"Sandy Brown"},{106,"Bleached Brown"},{71,"Schafter Purple"},{72,"Spinnaker Purple"},
        {142,"Midnight Purple"},{145,"Bright Purple"},{107,"Cream"},{111,"Ice White"},{112,"Frost White"},
        {37,"Classic Gold"},{139,"Green"},{144,"Hunter Green"},{125,"Securicor Green"},{157,"Epsilon Blue"},
        {140,"Fluorescent Blue"},{146,"V Dark Blue"},{127,"Police Blue"},{93,"Champagne"},{134,"Pure White"},
        {156,"Default Alloy"},{160,"Secret Gold"},
    });

    inline constexpr auto MatteColors = std::to_array<IndexedName>({
        {12,"Black"},{13,"Gray"},{14,"Light Gray"},{131,"Ice White"},{83,"Blue"},{82,"Dark Blue"},
        {84,"Midnight Blue"},{149,"Midnight Purple"},{148,"Schafter Purple"},{39,"Red"},{40,"Dark Red"},
        {41,"Orange"},{42,"Yellow"},{55,"Lime Green"},{128,"Green"},{151,"Forest Green"},
        {155,"Foliage Green"},{152,"Olive Drab"},{153,"Dark Earth"},{154,"Desert Tan"},{129,"Brown"},
    });

    inline constexpr auto MetalColors = std::to_array<IndexedName>({
        {117,"Brushed Steel"},{118,"Brushed Black Steel"},{119,"Brushed Aluminium"},{158,"Pure Gold"},{159,"Brushed Gold"},
    });

    inline constexpr auto UtilityColors = std::to_array<IndexedName>({
        {15,"Black"},{16,"Black Poly"},{17,"Dark Silver"},{18,"Silver"},{19,"Gun Metal"},{20,"Shadow Silver"},
        {43,"Red"},{44,"Bright Red"},{45,"Garnet Red"},{56,"Dark Green"},{57,"Green"},{75,"Dark Blue"},
        {76,"Midnight Blue"},{77,"Blue"},{78,"Sea Foam Blue"},{79,"Lightning Blue"},{80,"Maui Blue Poly"},
        {81,"Bright Blue"},{108,"Brown"},{109,"Medium Brown"},{110,"Light Brown"},{122,"Off White"},
    });

    inline constexpr auto WornColors = std::to_array<IndexedName>({
        {21,"Black"},{22,"Graphite"},{23,"Silver Grey"},{24,"Silver"},{25,"Blue Silver"},{26,"Shadow Silver"},
        {46,"Red"},{47,"Golden Red"},{48,"Dark Red"},{58,"Dark Green"},{59,"Green"},{60,"Sea Wash"},
        {85,"Dark Blue"},{86,"Blue"},{87,"Baby Blue"},{113,"Honey Beige"},{114,"Brown"},{115,"Dark Brown"},
        {116,"Straw Beige"},{121,"Off White"},{123,"Orange"},{124,"Light Orange"},{126,"Taxi Yellow"},
        {130,"Pale Orange"},{132,"White"},{133,"Olive Army Green"},
    });

    inline constexpr auto ChameleonColors = std::to_array<IndexedName>({
        {161,"Anodized Red"},{162,"Anodized Wine"},{163,"Anodized Purple"},{164,"Anodized Blue"},{165,"Anodized Green"},
        {166,"Anodized Lime"},{167,"Anodized Copper"},{168,"Anodized Bronze"},{169,"Anodized Champagne"},{170,"Anodized Gold"},
        {171,"Green Blue Flip"},{172,"Green Red Flip"},{173,"Green Brown Flip"},{174,"Green Turquoise Flip"},{175,"Green Purple Flip"},
        {176,"Teal Purple Flip"},{177,"Turquoise Red Flip"},{178,"Turquoise Purple Flip"},{179,"Cyan Purple Flip"},{180,"Blue Pink Flip"},
        {181,"Blue Green Flip"},{182,"Purple Red Flip"},{183,"Purple Green Flip"},{184,"Magenta Green Flip"},{185,"Magenta Yellow Flip"},
        {186,"Burgundy Green Flip"},{187,"Magenta Cyan Flip"},{188,"Copper Purple Flip"},{189,"Magenta Orange Flip"},{190,"Red Orange Flip"},
        {191,"Orange Purple Flip"},{192,"Orange Blue Flip"},{193,"White Purple Flip"},{194,"Red Rainbow Flip"},{195,"Blue Rainbow Flip"},
        {196,"Dark Green Pearl"},{197,"Dark Teal Pearl"},{198,"Dark Blue Pearl"},{199,"Dark Purple Pearl"},{200,"Oil Slick Pearl"},
        {201,"Light Green Pearl"},{202,"Light Blue Pearl"},{203,"Light Purple Pearl"},{204,"Light Pink Pearl"},{205,"Off White Prismatic"},
        {206,"Pink Pearl"},{207,"Yellow Pearl"},{208,"Green Pearl"},{209,"Blue Pearl"},{210,"Cream Pearl"},
        {211,"White Prismatic"},{212,"Graphite Prismatic"},{213,"Dark Blue Prismatic"},{214,"Dark Purple Prismatic"},{215,"Hot Pink Prismatic"},
        {216,"Red Prismatic"},{217,"Green Prismatic"},{218,"Black Prismatic"},{219,"Oil Slick Prismatic"},{220,"Black Rainbow"},
        {221,"Black Holographic"},{222,"White Holographic"},
    });

    inline constexpr auto ChromeColors = std::to_array<IndexedName>({IndexedName{120,"Chrome"}});

    inline constexpr std::array<IndexedName, 14> HeadlightColors{{
        {-1,"Default"},{0,"White"},{1,"Blue"},{2,"Electric Blue"},{3,"Mint Green"},{4,"Lime Green"},
        {5,"Yellow"},{6,"Golden Shower"},{7,"Orange"},{8,"Red"},{9,"Pony Pink"},{10,"Hot Pink"},{11,"Purple"},{12,"Blacklight"},
    }};

    inline constexpr std::array<RgbName, 13> NeonColors{{
        RgbName{"White",222,222,255}, RgbName{"Blue",2,21,255}, RgbName{"Electric Blue",3,83,255},
        RgbName{"Mint Green",0,255,140}, RgbName{"Lime Green",94,255,1}, RgbName{"Yellow",255,255,0},
        RgbName{"Golden Shower",255,150,5}, RgbName{"Orange",255,62,0}, RgbName{"Red",255,1,1},
        RgbName{"Pony Pink",255,50,100}, RgbName{"Hot Pink",255,5,190}, RgbName{"Purple",35,1,255},
        RgbName{"Blacklight",15,3,255},
    }};

    inline constexpr std::array<RgbName, 11> TireSmokeColors{{
        RgbName{"White",255,255,255}, RgbName{"Black",20,20,20}, RgbName{"Blue",0,174,239},
        RgbName{"Yellow",252,238,0}, RgbName{"Purple",100,79,142}, RgbName{"Orange",255,127,0},
        RgbName{"Green",114,204,114}, RgbName{"Red",226,6,6}, RgbName{"Pink",203,54,148},
        RgbName{"Brown",180,130,97}, RgbName{"Patriot / special",0,0,0},
    }};

    [[nodiscard]] constexpr std::span<const IndexedName> ColorsForPalette(Paint::PaintPalette palette) noexcept
    {
        using Paint::PaintPalette;
        switch (palette)
        {
        case PaintPalette::Chrome: return ChromeColors;
        case PaintPalette::Matte: return MatteColors;
        case PaintPalette::Metals: return MetalColors;
        case PaintPalette::Utility: return UtilityColors;
        case PaintPalette::Worn: return WornColors;
        case PaintPalette::Chameleon: return ChameleonColors;
        case PaintPalette::Normal:
        case PaintPalette::Metallic:
        case PaintPalette::Pearl:
        case PaintPalette::Classic:
        case PaintPalette::Alloy:
            return ClassicColors;
        }
        return ClassicColors;
    }
}
