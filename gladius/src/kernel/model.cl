
#define GETPARAM(index, offset)                                                                    \
    (cmds[i].args[index] < 0) ? out[-cmds[i].args[index] + offset]                                 \
                              : parameter[cmds[i].args[index] + offset]
#define GETPARAM2(arg) (arg < 0) ? out[-arg] : parameter[arg]
float4 model(float3 Begin_1_cs, PAYLOAD_ARGS)
{
    float out[256];
    out[1] = Begin_1_cs.x;
    out[2] = Begin_1_cs.y;
    out[3] = Begin_1_cs.z;
    for (int i = 0; i < sizeOfCmds; ++i)
    {
        if (cmds[i].type == 20)
        {
            out[cmds[i].output[0]] =
              sphere((float3) (GETPARAM(0, 0), GETPARAM(0, 1), GETPARAM(0, 2)), GETPARAM(1, 0));
        }
        if (cmds[i].type == 8)
        {
            float3 const newPos =
              translate3f((float3) (GETPARAM(0, 0), GETPARAM(0, 1), GETPARAM(0, 2)),
                          (float3) (GETPARAM(1, 0), GETPARAM(1, 1), GETPARAM(1, 2)));
            out[cmds[i].output[0]] = newPos.x;
            out[cmds[i].output[0] + 1] = newPos.y;
            out[cmds[i].output[0] + 2] = newPos.z;
        }
        if (cmds[i].type == 3)
        {
            float4 const uniteRes = uniteWithColor(
              (float4) (GETPARAM(0, 0), GETPARAM(0, 1), GETPARAM(0, 2), GETPARAM(1, 0)),
              (float4) (GETPARAM(2, 0), GETPARAM(2, 1), GETPARAM(2, 3), GETPARAM(3, 0)));
            out[cmds[i].output[0]] = uniteRes.x;
            out[cmds[i].output[0] + 1] = uniteRes.y;
            out[cmds[i].output[0] + 2] = uniteRes.z;
            out[cmds[i].output[1]] = uniteRes.w;
        }
        if (cmds[i].type == 4)
        {
            out[cmds[i].output[0]] = uniteSmooth(GETPARAM(0, 0), GETPARAM(1, 0), GETPARAM(2, 0));
        }
        struct Command ending = cmds[sizeOfCmds - 1];
        return (float4) (GETPARAM2(ending.args[0]),
                         GETPARAM2(ending.args[1]),
                         GETPARAM2(ending.args[2]),
                         GETPARAM2(ending.args[3]));



        float const dist1 = sphere(pos, 12.3f);
        float const dist2 = sphere(pos, 12.3f);


        float const dist3 = sphere(pos, 12.3f);
        float const dist4 = sphere(pos, 12.3f);
        float const dist5 = sphere(pos, 12.3f);
        float const dist6 = sphere(pos, 12.3f);
        float const dist7 = sphere(pos, 12.3f);

        float dist8 = unite(dist1, dist2);
    }