#include "saturation.glsl"
#include "colormanagement.glsl"

uniform vec4 modulation;

vec4 adjustOutput(vec4 result)
{

    result = encodingToNits(result, sourceNamedTransferFunction, sourceTransferFunctionParams.x, sourceTransferFunctionParams.y);
    result.rgb = (colorimetryTransform * vec4(result.rgb, 1.0)).rgb;

    result = adjustSaturation(result);
    result *= modulation;

    result.rgb = doTonemapping(result.rgb);
    result = nitsToDestinationEncoding(result);
    return result;
}

