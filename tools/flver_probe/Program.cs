using SoulsFormats;
// Usage: flverprobe FILE...  one line per file: status, version, layout sizes of buffers with Unk14 members.
foreach (var path in args)
{
    try
    {
        FLVER.LayoutMember.ProbeMaxBoneIndex = -1;
        var flver = FLVER2.Read(path);
        int palViol = 0, palMeshes = 0;
        foreach (var mesh in flver.Meshes)
        {
            int pal = mesh.BoneIndices.Count;
            int mx = 0;
            foreach (var v in mesh.Vertices) for (int k = 0; k < 4; k++) if (v.BoneIndices[k] > mx) mx = v.BoneIndices[k];
            palMeshes++;
            if (mx >= pal) palViol++;
        }
        int faces = 0, verts = 0, bad = 0, tanCount = 0, tanBad = 0, tanZero = 0;
        foreach (var mesh in flver.Meshes)
        {
            verts += mesh.Vertices.Count;
            foreach (var fs in mesh.FaceSets) faces += fs.Indices.Count;
            foreach (var v in mesh.Vertices)
                foreach (var tg in v.Tangents) { tanCount++; double tl = Math.Sqrt(tg.X*tg.X+tg.Y*tg.Y+tg.Z*tg.Z); if (Math.Abs(tl-1) > 0.02) { tanBad++; if (tl < 1e-6) tanZero++; } }
            foreach (var v in mesh.Vertices)
                if (!float.IsFinite(v.Position.X) || !float.IsFinite(v.Position.Y) || !float.IsFinite(v.Position.Z)) bad++;
        }
        var sem = new System.Collections.Generic.List<string>();
        foreach (var layout in flver.BufferLayouts) foreach (var m in layout) if ((uint)m.Type == 0x14) sem.Add(m.Semantic.ToString() + ":" + m.Index);
        Console.WriteLine($"{path}\tOK\tsem=" + string.Join(",", sem.Distinct()) + $"\tmeshes={flver.Meshes.Count}\tverts={verts}\tfaces={faces}\tnonfinite={bad}\ttangents={tanCount}\ttangent_nonunit={tanBad}\ttangent_zero={tanZero}\tbones={flver.Bones.Count}\tmax_bone_index={FLVER.LayoutMember.ProbeMaxBoneIndex}\tpalette_meshes={palMeshes}\tpalette_violations={palViol}\tfs_counts={string.Join(";", flver.Meshes.SelectMany(m => m.FaceSets).Select(f => f.Indices.Count))}");
        //\tver={flver.Header.Version:X}\tmeshes={flver.Meshes.Count}\tverts={verts}\tfaces={faces}\tnonfinite={bad}\ttangents={tanCount}\ttangent_nonunit={tanBad}\ttangent_zero={tanZero}");
    }
    catch (Exception ex)
    {
        Console.WriteLine($"{path}\tERR\t{ex.GetType().Name}: {ex.Message}");
    }
}

// Tangent encoding check over every collected 0x14 sample (4 bytes each).
var samples = FLVER.LayoutMember.Unk14Samples;
if (samples.Count > 0)
{
    int n = samples.Count;
    int signedOk = 0, unsignedOk = 0, wSignedPos = 0;
    var byteCounts = new int[256];
    var wCounts = new int[256];
    foreach (var b in samples)
    {
        double sx = (sbyte)b[0] / 127.0, sy = (sbyte)b[1] / 127.0, sz = (sbyte)b[2] / 127.0;
        double ux = b[0] / 255.0 * 2 - 1, uy = b[1] / 255.0 * 2 - 1, uz = b[2] / 255.0 * 2 - 1;
        double sl = Math.Sqrt(sx * sx + sy * sy + sz * sz), ul = Math.Sqrt(ux * ux + uy * uy + uz * uz);
        if (Math.Abs(sl - 1) < 0.05) signedOk++;
        if (Math.Abs(ul - 1) < 0.05) unsignedOk++;
        for (int i = 0; i < 4; i++) byteCounts[b[i]]++;
        wCounts[b[3]]++;
    }
    Console.WriteLine($"samples={n}\tsigned_unit_len(|L-1|<0.05)={signedOk / (double)n:F4}\tunsigned_unit_len={unsignedOk / (double)n:F4}");
    Console.WriteLine("w_byte_top=" + string.Join(",", Enumerable.Range(0, 256).OrderByDescending(v => wCounts[v]).Take(6).Select(v => $"{v}:{wCounts[v]}")));
    Console.WriteLine("all_byte_top=" + string.Join(",", Enumerable.Range(0, 256).OrderByDescending(v => byteCounts[v]).Take(8).Select(v => $"{v}:{byteCounts[v]}")));
}

// Search: which 3 of the 4 bytes form a unit vector, under two decodings.
var triples = new[] { (0,1,2), (0,1,3), (0,2,3), (1,2,3) };
foreach (var t in triples)
{
    int okS = 0, okO = 0;
    foreach (var b in samples)
    {
        double[] s3 = { (sbyte)b[t.Item1] / 127.0, (sbyte)b[t.Item2] / 127.0, (sbyte)b[t.Item3] / 127.0 };
        double[] o3 = { (b[t.Item1] - 128) / 127.0, (b[t.Item2] - 128) / 127.0, (b[t.Item3] - 128) / 127.0 };
        if (Math.Abs(Math.Sqrt(s3[0]*s3[0]+s3[1]*s3[1]+s3[2]*s3[2]) - 1) < 0.02) okS++;
        if (Math.Abs(Math.Sqrt(o3[0]*o3[0]+o3[1]*o3[1]+o3[2]*o3[2]) - 1) < 0.02) okO++;
    }
    Console.WriteLine($"triple {t.Item1}{t.Item2}{t.Item3}\tsigned={okS / (double)samples.Count:F4}\toffset128={okO / (double)samples.Count:F4}");
}

{
    var c0 = new int[256];
    foreach (var b in samples) c0[b[0]]++;
    Console.WriteLine("byte0_top=" + string.Join(",", Enumerable.Range(0, 256).OrderByDescending(v => c0[v]).Take(8).Select(v => $"{v}:{c0[v]}")));
    int signLike = 0;
    foreach (var b in samples) if (b[0] == 0 || b[0] == 127 || b[0] == 129) signLike++;
    Console.WriteLine($"byte0_in_0_127_129={signLike / (double)samples.Count:F4}");
}
