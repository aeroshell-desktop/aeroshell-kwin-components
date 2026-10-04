#include "saturation.glsl"
#include "colormanagement.glsl"

vec4 adjustBrightness(vec4 result)
{

    result = encodingToNits(result, sourceNamedTransferFunction, sourceTransferFunctionParams.x, sourceTransferFunctionParams.y);
    result.rgb = (colorimetryTransform * vec4(result.rgb, 1.0)).rgb;

    result = adjustSaturation(result);

    result.rgb = doTonemapping(result.rgb);
    result = nitsToDestinationEncoding(result);
    return result;
}

