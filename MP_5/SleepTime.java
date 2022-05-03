import java.io.IOException;
import java.util.StringTokenizer;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.IntWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;

public class SleepTime {

    public static class TokenizerMapper
            extends Mapper<Object, Text, Text, IntWritable>{

        private final static IntWritable one = new IntWritable(1);
        private Text word = new Text();

        public void map(Object key, Text value, Context context
        ) throws IOException, InterruptedException {
            StringTokenizer itr = new StringTokenizer(value.toString());

            String prevTime = "";
            boolean countThis = false;
            while (itr.hasMoreTokens()) {
                String token = itr.nextToken();

                if (token.equals("T") && itr.hasMoreTokens()){
                    token = itr.nextToken();
                    if (itr.hasMoreTokens()) {
                        token = itr.nextToken();
                        if (token.indexOf(':') != -1) {
                            if (token.length() == 8) {
                                prevTime = token.substring(0, 2);
                                countThis = true;
                                //context.write(word, one);
                            } else if (token.length() == 7) {
                                prevTime = token.substring(0, 1);
                                countThis = true;
                                //context.write(word, one);
                            }
                        }
                    }
                }

                if (token.equals("sleep") && countThis){
                    countThis = false;
                    word.set(prevTime);
                    context.write(word, one);
                }



            }
        }
    }

    public static class IntSumReducer
            extends Reducer<Text,IntWritable,Text,IntWritable> {
        private IntWritable result = new IntWritable();

        public void reduce(Text key, Iterable<IntWritable> values,
                           Context context
        ) throws IOException, InterruptedException {
            int sum = 0;
            for (IntWritable val : values) {
                sum += val.get();
            }
            result.set(sum);
            context.write(key, result);
        }
    }

    public static void main(String[] args) throws Exception {
        Configuration conf = new Configuration();
        Job job = Job.getInstance(conf, "sleep time");
        job.setJarByClass(SleepTime.class);
        job.setMapperClass(TokenizerMapper.class);
        job.setCombinerClass(IntSumReducer.class);
        job.setReducerClass(IntSumReducer.class);
        job.setOutputKeyClass(Text.class);
        job.setOutputValueClass(IntWritable.class);
        FileInputFormat.addInputPath(job, new Path(args[0]));
        FileOutputFormat.setOutputPath(job, new Path(args[1]));
        System.exit(job.waitForCompletion(true) ? 0 : 1);
    }
}

